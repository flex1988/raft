#ifndef __RAFT_SERVICE_IMPL__
#define __RAFT_SERVICE_IMPL__

#include "brpc/server.h"
#include "src/consensus_node_impl.h"

namespace raft
{
class RaftServiceImpl : public RaftService
{
public:
    RaftServiceImpl(ConsensusNodeImpl* node);

    ~RaftServiceImpl() {};

    void AppendEntries(google::protobuf::RpcController* ctrl,
                               const AppendEntriesRequest* request,
                               AppendEntriesResponse* response,
                               google::protobuf::Closure* done);
    void RequestVote(google::protobuf::RpcController* ctrl,
                             const RequestVoteRequest* request,
                             RequestVoteResponse* response,
                             google::protobuf::Closure* done);
private:
    ConsensusNodeImpl* mConsensusNode;
};
}

#endif // __RAFT_SERVICE_IMPL__