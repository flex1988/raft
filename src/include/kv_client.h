#ifndef __KV_CLIENT_H__
#define __KV_CLIENT_H__

#include <string>
#include <vector>

#include "butil/endpoint.h"
#include "src/include/slice.h"
#include "src/include/status.h"
#include "src/include/raft_types.h"
#include "src/raft_closure.h"

namespace raft
{

struct KVClientOptions
{
    std::vector<butil::EndPoint>    servers;
    std::vector<RawServerId>        serverIds;
};

class KVClient
{
public:
    virtual Status Open(const KVClientOptions& options) = 0;

    virtual void Close() = 0;

    virtual void Get(const Slice& k, std::string* v, RaftStatusClosure<KVClient>* done) = 0;

    virtual void Put(const Slice& k, const Slice& v, RaftStatusClosure<KVClient>* done) = 0;

    virtual void Delete(const Slice& k, RaftStatusClosure<KVClient>* done) = 0;
};

}

#endif  // __KV_CLIENT_H__