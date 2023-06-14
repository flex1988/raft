#include "memory_storage.h"
#include "src/include/status.h"

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
        return ERROR_MEMORYSTORAGE_COMPACTED;
    }
    if (high > LastIndex() + 1)
    {
        assert(0);
    }
    // only contains dummy entries
    if (mEntries.size() == 1)
    {
        return ERROR_MEMORYSTORAGE_UNAVAILABLE;
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
    if (first > entries[0]->index())
    {
        start = first - entries[0]->index();
    }

    uint64_t offset = entries[0]->index() - mEntries[0]->index();
    if (mEntries.size() > offset)
    {
        mEntries.resize(offset + entries.size());
        memcpy(&mEntries[offset + start], &entries[start], sizeof(void*) * (entries.size() - start));
    }
    else if (mEntries.size() == offset)
    {
        mEntries.resize(mEntries.size() + entries.size());
        memcpy(&mEntries[mEntries.size()], &entries[start], sizeof(void*) * (entries.size() - start));
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


}
