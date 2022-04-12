#include "brpc/server.h"
#include "src/consensus_node.h"

namespace raft
{
class RaftServiceImpl : public RaftService
{
public:
    RaftServiceImpl(ConsensusNode* node);
    virtual ~RaftServiceImpl() {};

    virtual void AppendEntries(google::protobuf::RpcController* ctrl,
                               const AppendEntriesRequest* request,
                               AppendEntriesResponse* response,
                               google::protobuf::Closure* done);
    virtual void RequestVote(google::protobuf::RpcController* ctrl,
                             const RequestVoteRequest* request,
                             RequestVoteResponse* response,
                             google::protobuf::Closure* done);
private:
    ConsensusNode* mConsensusNode;
};
}
