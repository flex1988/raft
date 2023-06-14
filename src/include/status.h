#pragma once

#include <string>

namespace raft
{

class Status
{
public:
    Status()
    : code(0), msg("")
    {
    }

    Status(int ret, const char* m)
    : code(ret), msg(m)
    {
    }

    bool IsOK() const { return code == 0; }

    int Code() const { return code; }

    bool operator==(const Status& other)
    {
        return this->code == other.Code();
    }

private:
    int code;
    std::string msg;
};



static Status RAFT_OK = Status(0, "");
static Status RAFT_ERROR = Status(-1, "default error");
static Status ERROR_PROPOSAL_DROPPED = Status(-1000, "error proposal dropped");
static Status ERROR_MEMORYSTORAGE_COMPACTED = Status(-1001, "requested index is unavailable due to compaction");
static Status ERROR_MEMORYSTORAGE_SNAP_OUTOF_DATE = Status(-1002, "requested index is older than the existing snapshot");
static Status ERROR_MEMORYSTORAGE_UNAVAILABLE = Status(-1003, "requested entry at index is unavailable");


}
