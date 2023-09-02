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
    if (!mConsensusNode->IsLeader())
    {
        response->set_retcode(ERROR.Code());
        return;
    }

    raft::ServerId id = mConsensusNode->Id();
    raft::ServerId* leaderId = response->mutable_leader();
    leaderId->set_ip(id.ip());
    leaderId->set_port(id.port());
    response->set_val("world!");
    response->set_retcode(0);
}

void KVServiceImpl::Put(google::protobuf::RpcController* ctrl,
            const PutRequest* request,
            PutResponse* response,
            google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
    if (!mConsensusNode->IsLeader())
    {
        response->set_retcode(ERROR.Code());
        return;
    }

    raft::ServerId id = mConsensusNode->Id();
    raft::ServerId* leaderId = response->mutable_leader();
    leaderId->set_ip(id.ip());
    leaderId->set_port(id.port());
    
    raft::Command cmd;
    cmd.set_op(raft::PUT);
    cmd.set_key(request->key());
    cmd.set_val(request->val());

    _guard.release();
    mConsensusNode->Submit(cmd,
            raft::NewStatusCallbackA1<KVServiceImpl, google::protobuf::Closure*>(
            this, done, &KVServiceImpl::onPutDone));
}

void KVServiceImpl::Del(google::protobuf::RpcController* ctrl,
            const DelRequest* request,
            DelResponse* response,
            google::protobuf::Closure* done)
{
    brpc::ClosureGuard _guard(done);
    if (!mConsensusNode->IsLeader())
    {
        response->set_retcode(ERROR.Code());
        return;
    }
    raft::ServerId id = mConsensusNode->Id();
    raft::ServerId* leaderId = response->mutable_leader();
    leaderId->set_ip(id.ip());
    leaderId->set_port(id.port());
}

void KVServiceImpl::onPutDone(google::protobuf::Closure* done, const Status& status)
{
    brpc::ClosureGuard _guard(done);
    LOG(INFO) << "On put done ret: " << status.Code();
}

}
