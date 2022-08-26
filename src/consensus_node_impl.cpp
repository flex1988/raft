#include "src/consensus_node_impl.h"
#include "src/include/util.h"
#include "src/include/raft_types.h"
#include "assert.h"

DECLARE_int32(request_vote_timeout_ms);
// The election timeout is randomized to be between 150ms and 300ms.
DECLARE_int32(election_timeout_ms_min);
DECLARE_int32(election_timeout_ms_max);
DECLARE_int32(heartbeat_timeout_ms);
DECLARE_int32(port);
DECLARE_string(listen_addr);

namespace raft
{

static std::string role2string(raft::RoleType type)
{
    if (type == raft::FOLLOWER)
    {
        return "FOLLOWER";
    }
    else if (type == raft::CANDIDATE)
    {
        return "CANDIDATE";
    }
    else if (type == raft::LEADER)
    {
        return "LEADER";
    }
    else
    {
        return "UNKNOWN";
    }
}

void ConsensusNodeImpl::becomeCandidate()
{
    LOG(INFO) << "start election now...";
    mRole = raft::CANDIDATE;
    mCurrentTerm++;
    mVotedFor = mOwnId;
    mRequestVoteSucc = 1;
    for (uint32_t i = 0; i < mPeers.size(); i++)
    {
        brpc::Controller* ctrl = new brpc::Controller;
        raft::RaftService_Stub stub(&mChannels[i]);
        raft::RequestVoteRequest request;
        request.set_term(mCurrentTerm);
        request.mutable_candidate_id()->set_ip(mOwnId.ip());
        request.mutable_candidate_id()->set_port(mOwnId.port());
        raft::LogEntry lastLog;
        if (!mLogs.empty())
        {
            lastLog = mLogs.back();
        }
        request.set_last_log_index(lastLog.index());
        request.set_last_log_term(lastLog.term());
        raft::RequestVoteResponse* response = new raft::RequestVoteResponse;
        stub.RequestVote(ctrl, &request, response, brpc::NewCallback(this, &ConsensusNodeImpl::onRequestVoteDone, ctrl, response, mCurrentTerm));
    }
    mElectionTimer.AddTimer(butil::milliseconds_from_now(RandomRange(FLAGS_election_timeout_ms_min, FLAGS_election_timeout_ms_max) * 10),
            brpc::NewCallback(this, &ConsensusNodeImpl::onElectionTimeout, mCurrentTerm));
}

void ConsensusNodeImpl::becomeLeader()
{
    LOG(INFO) << "Become leader now...";
    mRole = raft::LEADER;
    mHeartBeatTimer.AddTimer(butil::milliseconds_from_now(FLAGS_heartbeat_timeout_ms),
            brpc::NewCallback(this, &ConsensusNodeImpl::onHeartBeatTimeout));
}

void ConsensusNodeImpl::onRequestVoteDone(brpc::Controller* ctrl, raft::RequestVoteResponse* response, uint64_t term)
{
    std::unique_ptr<brpc::Controller> _ctrl(ctrl);
    std::unique_ptr<raft::RequestVoteResponse> _response(response);
    LOG(INFO) << "on request vote done term: " << response->term() << " term " << term << " voteGranted: " << response->vote_granted() << " cluster: " << mPeers.size();
    if (ctrl->Failed())
    {
        LOG(ERROR) << "request vote rpc error: " << ctrl->ErrorText();
        return;
    }
    if (response->term() > term)
    {
        becomeFollower(response->term());
    }
    else if (response->term() == term && response->vote_granted())
    {
        mRequestVoteSucc++;
        LOG(INFO) << "RequestVoteSucc: " << mRequestVoteSucc;
        if (mRequestVoteSucc > (mPeers.size() + 1) / 2)
        {
            LOG(INFO) << "win election, become leader term: " << mCurrentTerm << " vote " << mRequestVoteSucc;
            becomeLeader();
        }
    }
}

void ConsensusNodeImpl::onAppendEntriesDone(brpc::Controller* ctrl, raft::AppendEntriesResponse* response, AppendEntriesContext* context)
{
    std::unique_ptr<brpc::Controller> _ctrl(ctrl);
    std::unique_ptr<raft::AppendEntriesResponse> _response(response);
    std::unique_ptr<AppendEntriesContext> _context(context);
    if (ctrl->Failed())
    {
        LOG(ERROR) << "AppendEntries response error: " << ctrl->ErrorText();
        return;
    }

    if (response->term() > mCurrentTerm)
    {
        becomeFollower(response->term());
        return;
    }

    if (!response->success())
    {
        LOG(ERROR) << "appendentries failed";
        return;
    }

    mNextIndex[context->index] += context->entryCount;
    mMatchIndex[context->index] += context->entryCount;

    applyLogToStateMachine();
}

void ConsensusNodeImpl::applyLogToStateMachine()
{
    uint64_t latest = mMatchIndex[0];
    for (int j = 1; j < mPeers.size(); j++)
    {
        latest = std::min(latest, mMatchIndex[j]);
    }
    for (uint64_t i = mCommitIndex + 1; i <= latest; i++)
    {
        // apply log to state machine
    }
    mCommitIndex = latest;
}

void ConsensusNodeImpl::onHeartBeatDone(brpc::Controller* ctrl, raft::AppendEntriesResponse* response, uint64_t term)
{
    std::unique_ptr<brpc::Controller> _ctrl(ctrl);
    std::unique_ptr<raft::AppendEntriesResponse> _response(response);
    if (ctrl->Failed())
    {
        LOG(ERROR) << "Heart beat timeout error: " << ctrl->ErrorText();
        return;
    }
    if (!response->success())
    {
        if (response->term() > term)
        {
            becomeFollower(response->term());
        }
    }
}

void ConsensusNodeImpl::onElectionTimeout(uint64_t term)
{
    if (mRole != raft::FOLLOWER && mRole != raft::CANDIDATE)
    {
        LOG(INFO) << "during election timout role change to " << GetRoleStr();
        return;
    }
    if (term != mCurrentTerm)
    {
        LOG(INFO) << "during election timeout term change from " << term << " to " << mCurrentTerm;
        return;
    }
    becomeCandidate();
}

void ConsensusNodeImpl::onHeartBeatTimeout()
{
    LeaderSendHeartBeats();
}

void ConsensusNodeImpl::init()
{
    for (uint32_t i = 0; i < mPeers.size(); i++)
    {
        brpc::ChannelOptions options;
        options.timeout_ms = FLAGS_request_vote_timeout_ms;
        if (mChannels[i].Init(mPeers[i].ip().c_str(), mPeers[i].port(), &options) != 0)
        {
            LOG(ERROR) << "server init channel success server: " << mPeers[i].ip() << ":" << mPeers[i].port();
            return;
        }
    }
}

void ConsensusNodeImpl::AddPeer(raft::ServerId id)
{   
    if (id.ip() == FLAGS_listen_addr && id.port() == FLAGS_port)
    {
        mOwnId = id;
    }
    else
    {
        mPeers.push_back(id);
    }
}

std::string ConsensusNodeImpl::GetRoleStr()
{
    return role2string(mRole);
}

void ConsensusNodeImpl::becomeFollower(uint64_t term)
{
    LOG(INFO) << "Become follower now...";
    mCurrentTerm = term;
    mRole = raft::FOLLOWER;
    new (&mVotedFor) raft::ServerId;
    mElectionTimer.AddTimer(butil::milliseconds_from_now(RandomRange(FLAGS_election_timeout_ms_min, FLAGS_election_timeout_ms_max) * 10),
            brpc::NewCallback(this, &ConsensusNodeImpl::onElectionTimeout, mCurrentTerm));
}

void ConsensusNodeImpl::LeaderSendHeartBeats()
{
    if (mRole != raft::LEADER)
    {
        return;
    }

    for (uint32_t i = 0; i < mPeers.size(); i++)
    {
        brpc::Controller* ctrl = new brpc::Controller;
        raft::RaftService_Stub stub(&mChannels[i]);
        raft::AppendEntriesRequest request;
        raft::LogEntry lastLog;
        if (!mLogs.empty())
        {
            lastLog = mLogs.back();
        }
        request.set_term(mCurrentTerm);
        request.set_prev_log_index(lastLog.index());
        request.set_prev_log_term(lastLog.term());
        request.set_leader_commit(mCommitIndex);
        request.mutable_leader_id()->set_ip(mOwnId.ip());
        request.mutable_leader_id()->set_port(mOwnId.port());
        raft::AppendEntriesResponse* response = new raft::AppendEntriesResponse;
        stub.AppendEntries(ctrl, &request, response, brpc::NewCallback(this, &ConsensusNodeImpl::onHeartBeatDone, ctrl, response, mCurrentTerm));
    }
    mHeartBeatTimer.AddTimer(butil::milliseconds_from_now(FLAGS_heartbeat_timeout_ms),
            brpc::NewCallback(this, &ConsensusNodeImpl::onHeartBeatTimeout));
}

// 1. serialize
// 2. write to local storage
// 3. replicate to majority nodes
// 4. done->run
void ConsensusNodeImpl::Submit(const Command& cmd, RaftStatusClosure<ConsensusNodeImpl>* done)
{
    if (mRole != raft::LEADER)
    {
        done->SetStatus(Status::ERROR());
        done->Run();
        return;
    }
    raft::LogEntry entry;
    entry.set_index(mLogs.size());
    entry.set_term(mCurrentTerm);
    std::string val;
    cmd.SerializeToString(&val);
    entry.set_data(val.data());
    // TODO persistent storage
    mLogs.push_back(entry);
    mLatestLogIndex++;
    assert(mSubmitCallbacks.find(mLatestLogIndex) == mSubmitCallbacks.end());
    mSubmitCallbacks[mLatestLogIndex] = done;

    if (!mIsBackgroundAEOngoing)
    {
        run_closure_in_bthread(brpc::NewCallback(this, &ConsensusNodeImpl::leaderReplicateLog), BTHREAD_ATTR_NORMAL);
        mIsBackgroundAEOngoing = true;
    }
}

void ConsensusNodeImpl::onSubmitDone(const Status& status)
{

}

void ConsensusNodeImpl::leaderReplicateLog()
{
    mIsBackgroundAEOngoing = false;
    for (uint32_t i = 0; i < mPeers.size(); i++)
    {
        raft::ServerId id = mPeers[i];
        if (mLatestLogIndex > mNextIndex[i])
        {
            raft::RaftService_Stub stub(&mChannels[i]);
            raft::AppendEntriesRequest request;
            request.set_term(mCurrentTerm);
            request.mutable_leader_id()->set_ip(mOwnId.ip());
            request.mutable_leader_id()->set_port(mOwnId.port());

            if (mNextIndex[i] == 0)
            {
                request.set_prev_log_index(INVALID_LOG_INDEX);
                request.set_prev_log_term(INVALID_LOG_TERM);
            }
            else
            {
                raft::LogEntry& prev = mLogs[mNextIndex[i]];
                request.set_prev_log_index(prev.index());
                request.set_prev_log_term(prev.term());
            }

            request.set_leader_commit(mCommitIndex);
            for (uint64_t j = mNextIndex[i]; j < mLatestLogIndex; j++)
            {
                raft::LogEntry* entry = request.add_entries();
                entry->set_index(j);
                entry->set_term(mLogs[j].term());
                entry->set_data(mLogs[j].data());
            }
            brpc::Controller* ctrl = new brpc::Controller;
            raft::AppendEntriesResponse* response = new raft::AppendEntriesResponse;
            AppendEntriesContext* context = new AppendEntriesContext;
            context->index = i;
            context->entryCount = request.entries_size();
            stub.AppendEntries(ctrl, &request, response, brpc::NewCallback(this, &ConsensusNodeImpl::onAppendEntriesDone, ctrl, response, context));
        }
    }
}

void ConsensusNodeImpl::HandleAppendEntries(google::protobuf::RpcController* ctrl,
                                    const AppendEntriesRequest* request,
                                    AppendEntriesResponse* response,
                                    google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
    response->set_term(mCurrentTerm);
    if (request->term() < mCurrentTerm)
    {
        LOG(ERROR) << "appendentries failed term " << request->term() << " self term " << mCurrentTerm;
        response->set_success(false);
        return;
    }
    // TODO compare log
    LOG(INFO) << "appendentries term " << request->term();
    mCurrentTerm = request->term();
    mElectionTimer.AddTimer(butil::milliseconds_from_now(RandomRange(FLAGS_election_timeout_ms_min, FLAGS_election_timeout_ms_max) * 10),
            brpc::NewCallback(this, &ConsensusNodeImpl::onElectionTimeout, mCurrentTerm));
    if (mRole != raft::FOLLOWER)
    {
        becomeFollower(mCurrentTerm);
    }
    response->set_success(true);
}

void ConsensusNodeImpl::HandleRequestVote(google::protobuf::RpcController* ctrl,
                            const RequestVoteRequest* request,
                            RequestVoteResponse* response,
                            google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
    if (request->term() > mCurrentTerm)
    {
        becomeFollower(request->term());
    }
    response->set_term(mCurrentTerm);
    if (request->term() < mCurrentTerm || mRole != raft::FOLLOWER)
    {
        LOG(ERROR) << "reject request vote request term " << request->term() 
                << " current term " << mCurrentTerm << " Current role: " << GetRoleStr();
        response->set_vote_granted(false);
        return;
    }
    // TODO remove election timeout timer
    // already voted for some other candidate
    if (mCurrentTerm == request->term() && !mVotedFor.ip().empty())
    {
        LOG(ERROR) << "reject request vote cause already vote for";
        response->set_vote_granted(false);
        return;
    }
    // if (mConsensusNode->GetLastLogEntry() != NULL && (request->last_log_term() < mConsensusNode->GetLastLogEntry()->term()
    //         || request->last_log_index() < mConsensusNode->GetLastLogEntry()->index()))
    // {
    //     LOG(ERROR) << "reject request vote lower log term or index";
    //     response->set_vote_granted(false);
    //     return;
    // }
    LOG(INFO) << "approve request vote for " << request->candidate_id().ip() << ":" << request->candidate_id().port();
    mCurrentTerm = request->term();
    mElectionTimer.AddTimer(butil::milliseconds_from_now(RandomRange(FLAGS_election_timeout_ms_min, FLAGS_election_timeout_ms_max) * 10),
            brpc::NewCallback(this, &ConsensusNodeImpl::onElectionTimeout, mCurrentTerm));
    mVotedFor = request->candidate_id();
    response->set_vote_granted(true);
}

}