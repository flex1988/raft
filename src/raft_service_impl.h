#include "brpc/server.h"
#include "raft.pb.h"

namespace raft
{
class RaftServiceImpl : public RaftService
{
public:
    RaftServiceImpl() {};
    virtual ~RaftServiceImpl {};

    virtual void AppendEntries();
    virtual void RequestVote();
};
}
