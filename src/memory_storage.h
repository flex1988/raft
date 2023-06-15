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

    Status Entries(uint64_t start, uint64_t end, std::vector<raft::LogEntry*>& entries);

    uint64_t Term(uint64_t i);

    uint64_t LastIndex();

    uint64_t FirstIndex();

    uint64_t firstIndex() { return (*mEntries.begin())->index() + 1; }

    void Snapshot() { }

    Status CreateSnapshot(uint64_t i, raft::ConfState* cs, char* data, raft::Snapshot* snapshot);

    void Append(const std::vector<raft::LogEntry*>& entries);

private:
    std::vector<raft::LogEntry*> mEntries;
    raft::Snapshot               mSnapshot;
};
}

#endif // __STORAGE__
