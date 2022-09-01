#ifndef __KV_SERVICE_IMPL__
#define __KV_SERVICE_IMPL__

#include "brpc/server.h"
#include "src/consensus_node_impl.h"
#include "src/proto/kv.pb.h"

namespace raft
{

class KVServiceImpl : public KVService
{
public:
    KVServiceImpl(ConsensusNodeImpl* node);
    ~KVServiceImpl();

    void Get(google::protobuf::RpcController* ctrl,
             const GetRequest* request,
             GetResponse* response,
             google::protobuf::Closure* done);

    void Put(google::protobuf::RpcController* ctrl,
             const PutRequest* request,
             PutResponse* response,
             google::protobuf::Closure* done);

    void Del(google::protobuf::RpcController* ctrl,
             const DelRequest* request,
             DelResponse* response,
             google::protobuf::Closure* done);

private:
    void onPutDone(google::protobuf::Closure* done, const Status& status);

private:
    ConsensusNodeImpl* mConsensusNode;
};

}

#endif // __KV_SERVICE_IMPL__