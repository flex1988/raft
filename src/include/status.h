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

    Status(int ret)
    : code(ret)
    {
    }

    bool IsOK() const { return code == 0; }

    int Code() const { return code; }

    static Status OK() { return {}; }

    static Status ERROR() { return {-1}; }

private:
    int code;
    std::string msg;
};
}
