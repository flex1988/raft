#pragma once

#include <string>
#include <vector>
#include "src/include/slice.h"
#include "src/include/status.h"

namespace raft
{

struct KVClientOptions
{
    std::vector<butil::EndPoint> servers;
};

class KVClient
{
public:
    KVClient() = default;

    KVClient& operator=(const KVClient&) = delete;

    virtual ~KVClient();

    virtual Status Open(const KVClientOptions& options) = 0;

    virtual Status Close() = 0;

    virtual Status Get(const Slice& k, std::string* v) = 0;

    virtual Status Put(const Slice& k, const Slice& v) = 0;

    virtual Status Delete(const Slice& k) = 0;
};

}