#ifndef __RAFT_TYPES_H__
#define __RAFT_TYPES_H__

#include <string>

namespace raft
{

const uint64_t INVALID_LOG_INDEX = -1;
const uint64_t INVALID_LOG_TERM = -1;


struct PeerId
{
    int         port;
    std::string ip;

    PeerId()
    : port(0)
    {
    }

    PeerId(std::string& ip, int port)
    : port(port), ip(ip)
    {
    }
};

struct RawServerId
{
    int         port;
    std::string ip;

    RawServerId()
    : port(0)
    {
    }

    RawServerId(const std::string& ip, int port)
    : port(port), ip(ip)
    {
    }

    bool operator == (const RawServerId& other)
    {
        return port == other.port && ip == other.ip;
    }
};

}

#endif // __RAFT_TYPES_H__