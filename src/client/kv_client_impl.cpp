#include "src/proto/kv.pb.h"
#include "src/client/kv_client_impl.h"

namespace raft
{

Status KVClientImpl::Open(const KVClientOptions& options)
{
    mServerCount = options.servers.size();
    for (int i = 0; i < mServerCount; i++)
    {
        brpc::ChannelOptions channeloption;
        if (mChannels[i].Init(options.servers[i], &channeloption) != 0)
        {
            LOG(ERROR) << "server init channel success server: " << butil::endpoint2str(options.servers[i]);
            return Status::ERROR();
        }
        mServerIds.push_back(options.serverIds[i]);
    }
    return Status::OK();
}

void KVClientImpl::Close()
{
}

void KVClientImpl::Get(const Slice& k, std::string* v, RaftStatusClosure<KVClient>* done)
{
    if (done == NULL)
    {
        int index = mLeaderIndex;
        if (index == -1)
        {
            index = rand() % mServerCount;
        }
        brpc::Controller ctrl;
        raft::KVService_Stub stub(&mChannels[index]);
        raft::GetRequest request;
        request.set_key(k.data, k.size);
        raft::GetResponse response;
        stub.Get(&ctrl, &request, &response, NULL);
        *v = response.val();
        if (response.has_leader())
        {
            RawServerId id(response.leader().ip(), response.leader().port());
            for (int i = 0; i < mServerCount; i++)
            {
                if (mServerIds[i] == id)
                {
                    mLeaderIndex = i;
                    break;
                }
            }
        }
        else
        {
            mLeaderIndex = -1;
        }
    }
    else
    {

    }
}

void KVClientImpl::Put(const Slice& k, const Slice& v, RaftStatusClosure<KVClient>* done)
{
    if (done == NULL)
    {
        int index = mLeaderIndex;
        if (index == -1)
        {
            index = rand() % mServerCount;
        }
        brpc::Controller ctrl;
        raft::KVService_Stub stub(&mChannels[index]);
        raft::PutRequest request;
        request.set_key(k.data, k.size);
        request.set_val(v.data, v.size);
        raft::PutResponse response;
        stub.Put(&ctrl, &request, &response, NULL);
        if (response.has_leader())
        {
            RawServerId id(response.leader().ip(), response.leader().port());
            for (int i = 0; i < mServerCount; i++)
            {
                if (mServerIds[i] == id)
                {
                    mLeaderIndex = i;
                    break;
                }
            }
        }
        else
        {
            mLeaderIndex = -1;
        }
    }
}

void KVClientImpl::Delete(const Slice& k, RaftStatusClosure<KVClient>* done)
{

}

void KVClientImpl::onFinishGet(const Status& status)
{

}


}