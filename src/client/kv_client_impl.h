#ifndef __KV_CLIENT_IMPL_H__
#define __KV_CLIENT_IMPL_H__

#include <brpc/channel.h>
#include "src/include/kv_client.h"
#include "src/include/slice.h"
#include "src/include/status.h"
#include "src/raft_closure.h"

namespace raft
{

class KVClientImpl : public KVClient
{
public:
    KVClientImpl()
    : mLeaderIndex(-1),
      mServerCount(0)
    {

    }

    virtual ~KVClientImpl() {};

    Status Open(const KVClientOptions& options);

    void Close();

    void Get(const Slice& k, std::string* v, RaftStatusClosure<KVClient>* done);

    void Put(const Slice& k, const Slice& v, RaftStatusClosure<KVClient>* done);

    void Delete(const Slice& k, RaftStatusClosure<KVClient>* done);

private:
    void onFinishGet(const Status&);
    void onFinishPut(const Status&);
    void onFinishDelete(const Status&);

private:
    brpc::Channel               mChannels[5];
    std::vector<RawServerId>    mServerIds;

    int                     mLeaderIndex;
    int                     mServerCount;
};

}

#endif  // __KV_CLIENT_IMPL_H__