#pragma once

#include <string>
#include <cstring>
#include <cstddef>

namespace raft
{

class Slice
{
public:
    Slice() : data(""), size(0) {}
    Slice(const char* d, size_t n) : data(d), size(n) {}
    Slice(const std::string& s) : data(s.data()), size(s.size()) {}
    Slice(const char* s) : data(s) { size = (s == NULL) ? 0 : strlen(s); }

    const char* data;
    size_t      size;
};

}