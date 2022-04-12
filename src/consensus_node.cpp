#include "src/consensus_node.h"
#include "src/util.h"
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

static void onElectionTimeout(void* arg)
{
    raft::ConsensusNode* server = reinterpret_cast<raft::ConsensusNode*>(arg);
    if (server->GetRole() != raft::FOLLOWER && server->GetRole() != raft::CANDIDATE)
    {
        LOG(INFO) << "during election timout role change to " << server->GetRoleStr();
        return;
    }
    if (server->GetElectionTerm() != server->GetCurrentTerm())
    {
        LOG(INFO) << "during election timeout term change from " << server->GetElectionTerm() << " to " << server->GetCurrentTerm();
        return;
    }
    server->StartElection();
}

static void onHeartBeatTimeout(void* arg)
{
    reinterpret_cast<raft::ConsensusNode*>(arg)->LeaderSendHeartBeats();
}

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

void ConsensusNode::StartElection()
{
    LOG(INFO) << "start election now...";
    mRole = raft::CANDIDATE;
    mCurrentTerm++;
    mVotedFor = mOwnId;
    mRequestVoteSucc = 1;
    for (int i = 0; i < mServerIds.size(); i++)
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
        stub.RequestVote(ctrl, &request, response, brpc::NewCallback(this, &ConsensusNode::onRequestVoteDone, ctrl, response, mCurrentTerm));
    }
    RefreshElectionTimer();
}

void ConsensusNode::StartLeader()
{
    LOG(INFO) << "Become leader now...";
    mRole = raft::LEADER;
    AddHeartBeatTimer();
}

void ConsensusNode::onRequestVoteDone(brpc::Controller* ctrl, raft::RequestVoteResponse* response, uint64_t term)
{
    std::unique_ptr<brpc::Controller> _ctrl(ctrl);
    std::unique_ptr<raft::RequestVoteResponse> _response(response);
    LOG(INFO) << "on request vote done term: " << response->term() << " term " << term << " voteGranted: " << response->vote_granted() << " cluster: " << mServerIds.size();
    if (ctrl->Failed())
    {
        LOG(ERROR) << "request vote rpc error: " << ctrl->ErrorText();
        return;
    }
    if (response->term() > term)
    {
        BecomeFollower(response->term());
    }
    else if (response->term() == term && response->vote_granted())
    {
        mRequestVoteSucc++;
        LOG(INFO) << "RequestVoteSucc: " << mRequestVoteSucc;
        if (mRequestVoteSucc > (mServerIds.size() + 1) / 2)
        {
            LOG(INFO) << "win election, become leader term: " << mCurrentTerm << " vote " << mRequestVoteSucc;
            StartLeader();
        }
    }
    else
    {

        LOG(ERROR) << "xxxx";
    }
}

void ConsensusNode::onHeartBeatDone(brpc::Controller* ctrl, raft::AppendEntriesResponse* response, uint64_t term)
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
            BecomeFollower(response->term());
        }
    }
}

void ConsensusNode::Init()
{
    for (uint32_t i = 0; i < mServerIds.size(); i++)
    {
        brpc::ChannelOptions options;
        options.timeout_ms = FLAGS_request_vote_timeout_ms;
        if (mChannels[i].Init(mServerIds[i].ip().c_str(), mServerIds[i].port(), &options) != 0)
        {
            LOG(ERROR) << "server init channel success server: " << mServerIds[i].ip() << ":" << mServerIds[i].port();
            return;
        }
    }
}

void ConsensusNode::AddServer(raft::ServerId id)
{    if (id.ip() == FLAGS_listen_addr && id.port() == FLAGS_port)
    {
        mOwnId = id;
    }
    else
    {
        mServerIds.push_back(id);
    }
}

std::string ConsensusNode::GetRoleStr()
{
    return role2string(mRole);
}

void ConsensusNode::BecomeFollower(uint64_t term)
{
    LOG(INFO) << "Become follower now...";
    mCurrentTerm = term;
    mRole = raft::FOLLOWER;
    new (&mVotedFor) raft::ServerId;
    RefreshElectionTimer();
}

void ConsensusNode::LeaderSendHeartBeats()
{
    if (mRole != raft::LEADER)
    {
        return;
    }

    for (int i = 0; i < mServerIds.size(); i++)
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
        stub.AppendEntries(ctrl, &request, response, brpc::NewCallback(this, &ConsensusNode::onHeartBeatDone, ctrl, response, mCurrentTerm));
    }
    AddHeartBeatTimer();
}

void ConsensusNode::AddElectionTimer()
{
    mElectionTimerTerm = mCurrentTerm;
    bthread_timer_add(&mElectionTimer,
            butil::milliseconds_from_now(RandomRange(FLAGS_election_timeout_ms_min, FLAGS_election_timeout_ms_max) * 10),
            onElectionTimeout,
            this);
}

void ConsensusNode::AddHeartBeatTimer()
{
    bthread_timer_add(&mHeartBeatTimer,
            butil::milliseconds_from_now(FLAGS_heartbeat_timeout_ms),
            onHeartBeatTimeout,
            this);
}

void ConsensusNode::RefreshElectionTimer()
{
    mElectionTimerTerm = mCurrentTerm;
    bthread_timer_del(mElectionTimer);
    bthread_timer_add(&mElectionTimer,
        butil::milliseconds_from_now(RandomRange(FLAGS_election_timeout_ms_min, FLAGS_election_timeout_ms_max) * 10),
        onElectionTimeout,
        this);
}

}