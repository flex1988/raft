#include <butil/logging.h>

#include "raft_unstable_log.h"

namespace raft
{

RaftUnstable::RaftUnstable()
: mSnapshot(NULL), mOffset(0), mSnapshotInProgress(false), mOffsetInProgress(0)
{
}

RaftUnstable::~RaftUnstable()
{

}

uint64_t RaftUnstable::maybeTerm(uint64_t i)
{
    if (i < mOffset)
    {
        if (mSnapshot != NULL && mSnapshot->meta().index() == i)
        {
            return mSnapshot->meta().term();
        }
        return 0;
    }
    uint64_t last = maybeLastIndex();
    if (i > last || last == 0)
    {
        return 0;
    }
    return mEntries[i - mOffset]->term;
}

uint64_t RaftUnstable::maybeLastIndex()
{
    if (!mEntries.empty())
    {
        return mOffset + mEntries.size() - 1;
    }
    if (mSnapshot != NULL)
    {
        return mSnapshot->meta().index();
    }
    return 0;
}

uint64_t RaftUnstable::maybeFirstIndex()
{
    if (mSnapshot != NULL)
    {
        return mSnapshot->meta().index() + 1;
    }
    return 0;
}

std::vector<LogEntry*> RaftUnstable::nextEntries()
{
    std::vector<LogEntry*> entries;
    uint64_t inProgress = mOffsetInProgress - mOffset;
    if (mEntries.size() == inProgress)
    {
        return entries;
    }
    for (uint i = inProgress; i < mEntries.size(); i++)
    {
        entries.push_back(mEntries[i]);
    }
    return entries;
}

raft::Snapshot* RaftUnstable::nextSnapshot()
{
    if (mSnapshot == NULL || mSnapshotInProgress)
    {
        return NULL;
    }

    return mSnapshot;
}

void RaftUnstable::acceptInProgress()
{
    if (!mEntries.empty())
    {
        mOffsetInProgress = mEntries.back()->index + 1;
    }
    if (mSnapshot != NULL)
    {
        mSnapshotInProgress = true;
    }
}

void RaftUnstable::stableTo(uint64_t i, uint64_t t)
{
    uint64_t term = maybeTerm(i);
    if (term == 0)
    {
        LOG(INFO) << "entry at index " << i << " missing from unstable log; ignoring";
        return;
    }
    if (i < mOffset)
    {
        LOG(INFO) << "entry at index " << i << " matched unstable snapshot; ignoring";
        return;
    }

    if (term != t)
    {
        LOG(INFO) << "entry mismatched with entry in unstable log; ignoring";
        return;
    }

    uint truncate = i + 1 - mOffset;
    std::vector<LogEntry*> newEntries;
    for (uint i = 0; i < mEntries.size(); i++)
    {
        if (i < truncate)
        {
            delete mEntries[i];
        }
        else
        {
            newEntries.push_back(mEntries[i]);
        }
    }
    mEntries = newEntries;
    mOffset = i + 1;
    mOffsetInProgress = std::max(mOffsetInProgress, mOffset);
}

// useless
void RaftUnstable::shrinkEntriesArray()
{
}

void RaftUnstable::stableSnapTo(uint64_t i)
{
    if (mSnapshot != NULL && mSnapshot->meta().index() == i)
    {
        mSnapshot = NULL;
        mSnapshotInProgress = false;
    }
}

void RaftUnstable::restore(raft::Snapshot* snapshot)
{
    mOffset = snapshot->meta().index() + 1;
    mOffsetInProgress = mOffset;
    for (uint i = 0; i < mEntries.size(); i++)
    {
        delete mEntries[i];
    }
    mEntries.clear();
    mSnapshot = snapshot;
    mSnapshotInProgress = false;
}

void RaftUnstable::truncateAndAppend(std::vector<LogEntry*> entries)
{
    uint64_t fromIndex = entries[0]->index;
    if (fromIndex == mOffset + mEntries.size())
    {
        mEntries.insert(mEntries.end(), entries.begin(), entries.end());
    }
    else if (fromIndex <= mOffset)
    {
        mEntries = entries;
        mOffset = fromIndex;
        mOffsetInProgress = mOffset;
        LOG(INFO) << "replace the unstable entries from index " << fromIndex;
    }
    else
    {
        mEntries = slice(mOffset, fromIndex);
        mEntries.insert(mEntries.end(), entries.begin(), entries.end());
        mOffsetInProgress = std::min(mOffsetInProgress, fromIndex);
    }
}


std::vector<LogEntry*> RaftUnstable::slice(uint64_t low, uint64_t high)
{
    mustCheckOutOfBounds(low, high);
    std::vector<LogEntry*> s;
    for (uint i = 0; i < mEntries.size(); i++)
    {
        if (i < low - mOffset || i >= high - mOffset)
        {
            delete mEntries[i];
        }
        else
        {
            s.push_back(mEntries[i]);
        }
    }
    return s;
}

// u.offset <= lo <= hi <= u.offset+len(u.entries)
void RaftUnstable::mustCheckOutOfBounds(uint64_t low, uint64_t high)
{
    CHECK_LE(low, high) << "invalid unstable.slice low: " << low << " high: " << high;

    uint64_t upper = mOffset + mEntries.size();
    if (low < mOffset || high > upper)
    {
        LOG(ERROR) << "unstable.slice out of bound";
        LOG_ASSERT(0);
    }
}

}