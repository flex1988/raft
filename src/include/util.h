#ifndef __UTIL__
#define __UTIL__

#include <string>
#include <vector>

namespace raft
{
    void SplitString(std::vector<std::string>& results, const std::string& str, std::string delimiter);
    int RandomRange(int begin, int end);
}

#endif // __UTIL__