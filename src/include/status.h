#pragma once

#include <string>

namespace raft
{

class Status
{
public:
    Status() {}
    Status(int ret)
    : code(ret)
    {}

    static Status OK() { return {}; }

    static Status ERROR() { return {-1}; }

private:
    int code;
    std::string msg;
};
}