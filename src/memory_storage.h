#ifndef __STORAGE__
#define __STORAGE__

#include "raft.h"
#include "src/proto/raft.pb.h"

namespace raft
{

class MemoryStorage : public Storage
{
public:
    MemoryStorage();

    void InitialState();

    Status Entries(uint64_t start, uint64_t end, uint64_t maxSize, std::vector<LogEntry*>& entries);

    Status Term(uint64_t i, uint64_t& term);

    uint64_t LastIndex();

    uint64_t FirstIndex();

    uint64_t firstIndex() { return (*mEntries.begin())->index + 1; }

    void Snapshot() { }

    Status CreateSnapshot(uint64_t i, raft::ConfState* cs, std::string* data, raft::Snapshot* snapshot);

    Status ApplySnapshot(const raft::Snapshot& snapshot);

    Status Compact(uint64_t compactIndex);

    void Append(const std::vector<LogEntry*>& entries);

private:
    std::vector<LogEntry*>          mEntries;
    raft::Snapshot                  mSnapshot;
};
}

#endif // __STORAGE__
