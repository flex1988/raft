#ifndef __STORAGE__
#define __STORAGE__

#include "src/include/raft.h"
#include "src/proto/raft.pb.h"

namespace raft
{

class MemoryStorage : public Storage
{
public:
    MemoryStorage();

    void InitialState();

    Status Entries(uint64_t start, uint64_t end, std::vector<raft::LogEntry*>& entries);

    uint64_t Term(uint64_t i);

    uint64_t LastIndex();

    uint64_t FirstIndex();

    uint64_t firstIndex() { return (*mEntries.begin())->index() + 1; }

    void Snapshot() { }

    void Append(const std::vector<raft::LogEntry*>& entries);

private:
    std::vector<raft::LogEntry*> mEntries;
};
}

#endif // __STORAGE__
