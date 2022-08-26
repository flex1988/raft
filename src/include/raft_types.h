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

}