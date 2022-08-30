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

    bool IsOK() { return code == 0; }

    static Status OK() { return {}; }

    static Status ERROR() { return {-1}; }

private:
    int code;
    std::string msg;
};
}