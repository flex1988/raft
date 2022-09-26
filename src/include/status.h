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

    Status(int ret, char* m)
    : code(ret), msg(m)
    {
    }

    bool IsOK() const { return code == 0; }

    int Code() const { return code; }

private:
    int code;
    std::string msg;
};

static Status RAFT_OK = Status(0, "");

static Status RAFT_ERROR = Status(-1, "default error");

static Status ERROR_PROPOSAL_DROPPED = Status(-1000, "error proposal dropped");

}
