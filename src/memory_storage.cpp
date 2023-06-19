#include <butil/logging.h>

#include "memory_storage.h"
#include "status.h"

namespace raft
{

MemoryStorage::MemoryStorage()
{
    raft::LogEntry* log = new raft::LogEntry;
    log->set_index(0);
    log->set_term(0);
    mEntries.push_back(log);
}

void MemoryStorage::InitialState()
{

}

Status MemoryStorage::Entries(uint64_t low, uint64_t high, std::vector<raft::LogEntry*>& entries)
{

    uint64_t offset = mEntries[0]->index();
    if (low <= offset)
    {
        return ERROR_MEMSTOR_COMPACTED;
    }
    if (high > LastIndex() + 1)
    {
        assert(0);
    }
    // only contains dummy entries
    if (mEntries.size() == 1)
    {
        return ERROR_MEMSTOR_UNAVAILABLE;
    }

    entries.reserve(high - low + 1);
    entries.insert(entries.begin(), mEntries.begin() + (low - offset), mEntries.begin() + (high - offset));
    return RAFT_OK;
}

void MemoryStorage::Append(const std::vector<raft::LogEntry*>& entries)
{
    if (entries.empty())
    {
        return;
    }
    uint64_t first = firstIndex();
    uint64_t last = entries[0]->index() + entries.size() - 1;
    // no new entry
    if (last < first)
    {
        return;
    }

    int start = 0;
    std::vector<raft::LogEntry*> truncate;
    if (first > entries[0]->index())
    {
        start = first - entries[0]->index();
    }

    uint64_t offset = entries[start]->index() - mEntries[0]->index();
    if (mEntries.size() > offset)
    {
        mEntries.resize(offset + entries.size() - start);
        memcpy(&mEntries[offset], &entries[start], sizeof(void*) * (entries.size() - start));
    }
    else if (mEntries.size() == offset)
    {
        int oldSize = mEntries.size();
        mEntries.resize(mEntries.size() + entries.size() - start);
        memcpy(&mEntries[oldSize], &entries[start], sizeof(void*) * (entries.size() - start));
    }
    else
    {
        assert(0);
    }
}

uint64_t MemoryStorage::Term(uint64_t i)
{
    uint64_t offset = mEntries[0]->index();
    if (i < offset)
    {
        return 0;
    }
    if (i - offset >= mEntries.size())
    {
        return 0;
    }
    return mEntries[i - offset]->term();
}

uint64_t MemoryStorage::FirstIndex()
{
    return firstIndex();
}

uint64_t MemoryStorage::LastIndex()
{
    return mEntries[0]->index() + mEntries.size() - 1;
}

Status MemoryStorage::CreateSnapshot(uint64_t i, raft::ConfState* cs, std::string* data, raft::Snapshot* snapshot)
{
    if (i <= mSnapshot.meta().index())
    {
        return ERROR_MEMSTOR_SNAP_OUTOFDATE;
    }

    uint64_t offset = mEntries[0]->index();
    CHECK_LE(i, LastIndex()) << "snapshot is out of bound lastindex";

    mSnapshot.mutable_meta()->set_index(i);
    mSnapshot.mutable_meta()->set_term(mEntries[i - offset]->term());
    if (cs != NULL)
    {
        mSnapshot.mutable_meta()->set_allocated_confstate(cs);
    }
    mSnapshot.set_allocated_data(data);
    snapshot->CopyFrom(mSnapshot);
    return RAFT_OK;
}

Status MemoryStorage::ApplySnapshot(const raft::Snapshot& snapshot)
{
    uint64_t currentSnapshotIndex = mSnapshot.meta().index();
    uint64_t snapshotIndex = snapshot.meta().index();
    if (currentSnapshotIndex >= snapshotIndex)
    {
        return ERROR_MEMSTOR_SNAP_OUTOFDATE;
    }
    mSnapshot.CopyFrom(snapshot);
    for (int i = 0; i < mEntries.size(); i++)
    {
        delete mEntries[i];
    }
    mEntries.clear();
    raft::LogEntry* log = new raft::LogEntry;
    log->set_term(snapshot.meta().term());
    log->set_index(snapshot.meta().index());
    mEntries.push_back(log);
    return RAFT_OK;
}

Status MemoryStorage::Compact(uint64_t compactIndex)
{
    uint64_t offset = mEntries[0]->index();
    if (compactIndex <= offset)
    {
        return ERROR_MEMSTOR_COMPACTED;
    }

    CHECK_LE(compactIndex, LastIndex()) << "compact is out of bound lastIndex";
    uint64_t i = compactIndex - offset;
    raft::LogEntry* log = new raft::LogEntry;
    log->set_index(mEntries[i]->index());
    log->set_term(mEntries[i]->term());
    std::vector<raft::LogEntry*> newEntries;
    newEntries.push_back(log);
    for (int j = 0; j < i; j++)
    {
        delete mEntries[j];
    }
    i++;
    for (; i < mEntries.size(); i++)
    {
        newEntries.push_back(mEntries[i]);
    }
    mEntries = newEntries;
    return RAFT_OK;
}


}