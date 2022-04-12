#include "src/raft_service_impl.h"

namespace raft
{

RaftServiceImpl::RaftServiceImpl(ConsensusNode* node)
: mConsensusNode(node)
{
}

void RaftServiceImpl::AppendEntries(google::protobuf::RpcController* ctrl,
                                    const AppendEntriesRequest* request,
                                    AppendEntriesResponse* response,
                                    google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
    response->set_term(mConsensusNode->GetCurrentTerm());
    if (request->term() < mConsensusNode->GetCurrentTerm())
    {
        LOG(ERROR) << "appendentries failed term " << request->term() << " self term " << mConsensusNode->GetCurrentTerm();
        response->set_success(false);
        return;
    }
    // TODO compare log
    LOG(INFO) << "appendentries term " << request->term();
    mConsensusNode->SetTerm(request->term());
    mConsensusNode->RefreshElectionTimer();
    if (mConsensusNode->GetRole() != raft::FOLLOWER)
    {
        mConsensusNode->BecomeFollower(mConsensusNode->GetCurrentTerm());
    }
    response->set_success(true);
}

void RaftServiceImpl::RequestVote(google::protobuf::RpcController* ctrl,
                                  const RequestVoteRequest* request,
                                  RequestVoteResponse* response,
                                  google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
    if (request->term() > mConsensusNode->GetCurrentTerm())
    {
        mConsensusNode->BecomeFollower(request->term());
    }
    response->set_term(mConsensusNode->GetCurrentTerm());
    if (request->term() < mConsensusNode->GetCurrentTerm() || mConsensusNode->GetRole() != raft::FOLLOWER)
    {
        LOG(ERROR) << "reject request vote request term " << request->term() 
                << " current term " << mConsensusNode->GetCurrentTerm() << " Current role: " << mConsensusNode->GetRoleStr();
        response->set_vote_granted(false);
        return;
    }
    // TODO remove election timeout timer
    // already voted for some other candidate
    if (mConsensusNode->GetCurrentTerm() == request->term() && mConsensusNode->HasVotedFor())
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
    mConsensusNode->SetTerm(request->term());
    mConsensusNode->RefreshElectionTimer();
    mConsensusNode->SetVotedFor(request->candidate_id());
    response->set_vote_granted(true);
}

}
