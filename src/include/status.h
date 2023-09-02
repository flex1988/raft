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

extern Status OK;
extern Status ERROR;
extern Status ERROR_PROPOSAL_DROPPED;
extern Status ERROR_COMPACTED;
extern Status ERROR_UNAVAILABLE;
extern Status ERROR_SNAP_OUTOFDATE;

}
