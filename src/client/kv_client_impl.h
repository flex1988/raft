#include <brpc/channel.h>
#include "src/include/kv_client.h"
#include "src/include/slice.h"
#include "src/include/status.h"

namespace raft
{

class KVClientImpl : public KVClient
{
public:
    KVClientImpl();

    ~KVClientImpl();

    Status Open(const KVClientOptions& options);

    Status Close();

    Status Get(const Slice& k, std::string* v);

    Status Put(const Slice& k, const Slice& v);

    Status Delete(const Slice& k);

private:
    brpc::Channel mChannels[5];
};

}