#pragma once

#include <string>
#include <cstring>
#include <cstddef>

namespace raft
{

class Slice
{
public:
    Slice() : mData(""), mSize(0) {}
    Slice(const char* d, size_t n) : mData(d), mSize(n) {}
    Slice(const std::string& s) : mData(s.data()), mSize(s.size()) {}
    Slice(const char* s) : mData(s) { mSize = (s == NULL) ? 0 : strlen(s); }

private:
    const char* mData;
    size_t      mSize;
};

}