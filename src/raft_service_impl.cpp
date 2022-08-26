#include "src/raft_service_impl.h"

namespace raft
{

RaftServiceImpl::RaftServiceImpl(ConsensusNodeImpl* node)
: mConsensusNode(node)
{
}

void RaftServiceImpl::AppendEntries(google::protobuf::RpcController* ctrl,
                                    const AppendEntriesRequest* request,
                                    AppendEntriesResponse* response,
                                    google::protobuf::Closure* done)
{
    mConsensusNode->HandleAppendEntries(ctrl, request, response, done);
}

void RaftServiceImpl::RequestVote(google::protobuf::RpcController* ctrl,
                                  const RequestVoteRequest* request,
                                  RequestVoteResponse* response,
                                  google::protobuf::Closure* done)
{
    mConsensusNode->HandleRequestVote(ctrl, request, response, done);
}

}
