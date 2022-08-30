#include "src/kv_service_impl.h"

namespace raft
{

KVServiceImpl::KVServiceImpl(ConsensusNodeImpl* node)
: mConsensusNode(node)
{
}

KVServiceImpl::~KVServiceImpl()
{
}

void KVServiceImpl::Get(google::protobuf::RpcController* ctrl,
            const GetRequest* request,
            GetResponse* response,
            google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
    response->set_val("world!");
    response->set_retcode(0);
}

void KVServiceImpl::Put(google::protobuf::RpcController* ctrl,
            const PutRequest* request,
            PutResponse* response,
            google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
}

void KVServiceImpl::Del(google::protobuf::RpcController* ctrl,
            const DelRequest* request,
            DelResponse* response,
            google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
}

}