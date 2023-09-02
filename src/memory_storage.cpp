#include <butil/logging.h>

#include "memory_storage.h"
#include "status.h"

namespace raft
{

MemoryStorage::MemoryStorage()
{
    LogEntry* log = new LogEntry;
    mEntries.push_back(log);
}

void MemoryStorage::InitialState()
{

}

Status MemoryStorage::Entries(uint64_t low, uint64_t high, std::vector<LogEntry*>& entries)
{

    uint64_t offset = mEntries[0]->index;
    if (low <= offset)
    {
        return ERROR_COMPACTED;
    }
    if (high > LastIndex() + 1)
    {
        assert(0);
    }
    // only contains dummy entries
    if (mEntries.size() == 1)
    {
        return ERROR_UNAVAILABLE;
    }

    entries.reserve(high - low + 1);
    entries.insert(entries.begin(), mEntries.begin() + (low - offset), mEntries.begin() + (high - offset));
    return OK;
}

void MemoryStorage::Append(const std::vector<LogEntry*>& entries)
{
    if (entries.empty())
    {
        return;
    }
    uint64_t first = firstIndex();
    uint64_t last = entries[0]->index + entries.size() - 1;
    // no new entry
    if (last < first)
    {
        return;
    }

    int start = 0;
    std::vector<LogEntry*> truncate;
    if (first > entries[0]->index)
    {
        start = first - entries[0]->index;
    }

    uint64_t offset = entries[start]->index - mEntries[0]->index;
    if (mEntries.size() > offset)
    {
        mEntries.resize(offset + entries.size() - start);
        memcpy(&mEntries[offset], &entries[start], sizeof(void*) * (entries.size() - start));
    }
    else if (mEntries.size() == offset)
    {
        mEntries.insert(mEntries.end(), entries.begin() + start, entries.end());
    }
    else
    {
        assert(0);
    }
}

uint64_t MemoryStorage::Term(uint64_t i)
{
    uint64_t offset = mEntries[0]->index;
    if (i < offset)
    {
        return 0;
    }
    if (i - offset >= mEntries.size())
    {
        return 0;
    }
    return mEntries[i - offset]->term;
}

uint64_t MemoryStorage::FirstIndex()
{
    return firstIndex();
}

uint64_t MemoryStorage::LastIndex()
{
    return mEntries[0]->index + mEntries.size() - 1;
}

Status MemoryStorage::CreateSnapshot(uint64_t i, raft::ConfState* cs, std::string* data, raft::Snapshot* snapshot)
{
    if (i <= mSnapshot.meta().index())
    {
        return ERROR_SNAP_OUTOFDATE;
    }

    uint64_t offset = mEntries[0]->index;
    CHECK_LE(i, LastIndex()) << "snapshot is out of bound lastindex";

    mSnapshot.mutable_meta()->set_index(i);
    mSnapshot.mutable_meta()->set_term(mEntries[i - offset]->term);
    if (cs != NULL)
    {
        mSnapshot.mutable_meta()->set_allocated_confstate(cs);
    }
    mSnapshot.set_allocated_data(data);
    snapshot->CopyFrom(mSnapshot);
    return OK;
}

Status MemoryStorage::ApplySnapshot(const raft::Snapshot& snapshot)
{
    uint64_t currentSnapshotIndex = mSnapshot.meta().index();
    uint64_t snapshotIndex = snapshot.meta().index();
    if (currentSnapshotIndex >= snapshotIndex)
    {
        return ERROR_SNAP_OUTOFDATE;
    }
    mSnapshot.CopyFrom(snapshot);
    for (int i = 0; i < mEntries.size(); i++)
    {
        delete mEntries[i];
    }
    mEntries.clear();
    LogEntry* log = new LogEntry;
    log->term = snapshot.meta().term();
    log->index = snapshot.meta().index();
    mEntries.push_back(log);
    return OK;
}

Status MemoryStorage::Compact(uint64_t compactIndex)
{
    uint64_t offset = mEntries[0]->index;
    if (compactIndex <= offset)
    {
        return ERROR_COMPACTED;
    }

    CHECK_LE(compactIndex, LastIndex()) << "compact is out of bound lastIndex";
    uint64_t i = compactIndex - offset;
    LogEntry* log = new LogEntry;
    log->index = mEntries[i]->index;
    log->term = mEntries[i]->term;
    std::vector<LogEntry*> newEntries;
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
    return OK;
}


}