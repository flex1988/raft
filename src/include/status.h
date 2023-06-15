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

extern Status RAFT_OK;
extern Status RAFT_ERROR;
extern Status ERROR_PROPOSAL_DROPPED;
extern Status ERROR_MEMSTOR_COMPACTED;
extern Status ERROR_MEMSTOR_UNAVAILABLE;
extern Status ERROR_MEMSTOR_SNAP_OUTOFDATE;

}
