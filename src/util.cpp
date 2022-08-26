#include "src/include/util.h"

namespace raft
{
    void SplitString(std::vector<std::string>& results, const std::string& str, std::string delimiter)
    {
        size_t next = 0;
        size_t last = 0;
        while ((next = str.find(delimiter, last)) != std::string::npos)
        {
           results.push_back(str.substr(last, next - last));
           last = next + 1;
        }
        results.push_back(str.substr(last));
    }

    int RandomRange(int begin, int end)
    {
        return begin + rand() % (end - begin);
    }
}