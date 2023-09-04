#include "raft_log.h"
#include "status.h"

#include <butil/logging.h>
#include <vector>

namespace raft
{

RaftLog::RaftLog(Storage* stor, uint64_t maxApplyingEntsSize)
:   mMaxApplyingEntsSize(maxApplyingEntsSize),
    mApplyingEntsSize(0)
{
    assert(stor);
    mStorage.reset(stor);

    uint64_t firstIndex = mStorage->FirstIndex();
    // assert(firstIndex > 0);

    uint64_t lastIndex = mStorage->LastIndex();
    // assert(lastIndex > 0);

    mUnstable.reset(new RaftUnstable);
    mUnstable->mOffset = lastIndex + 1;
    mUnstable->mOffsetInProgress = lastIndex + 1;

    mCommitted = firstIndex - 1;
    mApplying = firstIndex - 1;
    mApplied = firstIndex - 1;
}

bool RaftLog::matchTerm(uint64_t i, uint64_t term)
{
    uint64_t t = 0;
    Status s = this->term(i, t);
    if (!s.IsOK())
    {
        return false;
    }
    return t == term;
}

Status RaftLog::term(uint64_t i, uint64_t& term)
{
    bool ret = mUnstable->maybeTerm(i, term);
    if (ret)
    {
        return OK;
    }
    LOG(INFO) << " i + 1 = " << i + 1 << " first index : " << firstIndex(); 
    if (i + 1 < firstIndex())
    {
        term = 0;
        return ERROR_COMPACTED;
    }

    if (i > lastIndex())
    {
        term = 0;
        return ERROR_UNAVAILABLE;
    }

    Status s = mStorage->Term(i, term);
    if (s.IsOK())
    {
        return OK;
    }
    if (s.Code() == ERROR_COMPACTED.Code() || s.Code() == ERROR_UNAVAILABLE.Code())
    {
        term = 0;
        return s;
    }
    assert(0);
}

// findConflict finds the index of the conflict.
// It returns the first pair of conflicting entries between the existing
// entries and the given entries, if there are any.
// If there is no conflicting entries, and the existing entries contains
// all the given entries, zero will be returned.
// If there is no conflicting entries, but the given entries contains new
// entries, the index of the first new entry will be returned.
// An entry is considered to be conflicting if it has the same index but
// a different term.
// The index of the given entries MUST be continuously increasing.
uint64_t RaftLog::findConflict(std::vector<LogEntry*> ents)
{
    for (uint32_t i = 0; i < ents.size(); i++)
    {
        LogEntry* entry = ents[i];
        if (matchTerm(entry->index, entry->term) == 0)
        {
            if (entry->index <= lastIndex())
            {
                uint64_t term = 0;
                this->term(entry->index, term);
                LOG(INFO) << "found conflict at index " << entry->index << " [existing term: " << zeroTermOnOutOfBounds(term)
                    << ", conflicting term: " << entry->term << "]";
            }

            return entry->index;
        }
    }
    return 0;
}

// findConflictByTerm returns a best guess on where this log ends matching
// another log, given that the only information known about the other log is the
// (index, term) of its single entry.
//
// Specifically, the first returned value is the max guessIndex <= index, such
// that term(guessIndex) <= term or term(guessIndex) is not known (because this
// index is compacted or not yet stored).
//
// The second returned value is the term(guessIndex), or 0 if it is unknown.
//
// This function is used by a follower and leader to resolve log conflicts after
// an unsuccessful append to a follower, and ultimately restore the steady flow
// of appends.
std::pair<uint64_t, uint64_t> RaftLog::findConflictByTerm(uint64_t index, uint64_t term)
{
    for (; index > 0; index--)
    {
        // If there is an error (likely ErrCompacted or ErrUnavailable), we don't
		// know whether it's a match or not, so assume a possible match and return
		// the index, with 0 term indicating an unknown term.
        uint64_t t = 0;
        Status s = this->term(index, t);
        if (!s.IsOK())
        {
            return {index, 0};
        }
        else if (t <= term)
        {
            return {index, t};
        }
    }
    return { 0, 0 };
}

uint64_t RaftLog::zeroTermOnOutOfBounds(uint64_t t)
{
    return t;
}

void RaftLog::commitTo(uint64_t tocommit)
{
    if (mCommitted < tocommit)
    {
        if (lastIndex() < tocommit)
        {
            assert(0);
        }
        mCommitted = tocommit;
    }
}

// maybeAppend returns (0, false) if the entries cannot be appended. Otherwise,
// it returns (last index of new entries, true).
bool RaftLog::maybeAppend(uint64_t index, uint64_t term, uint64_t committed, std::vector<LogEntry*> ents, uint64_t& lasti)
{
    if (!matchTerm(index, term))
    {
        lasti = 0;
        return false; 
    }

    uint64_t lastnewi = index + ents.size();
    uint64_t conflict = findConflict(ents);
    if (conflict == 0)
    {
        ;
    }
    else if (conflict <= mCommitted)
    {
        assert(0);
    }
    else
    {
        uint64_t offset = index + 1;
        if (conflict - offset > ents.size())
        {
            assert(0);
        }
        std::vector<LogEntry*> toappend;
        toappend.resize(ents.size() - (conflict - offset));
        toappend.insert(toappend.begin(), ents.begin() + conflict - offset, ents.end());
        append(toappend);
    }

    commitTo(std::min(committed, lastnewi));
    lasti = lastnewi;

    return true;
}


uint64_t RaftLog::append(std::vector<LogEntry*> ents)
{
    if (ents.empty())
    {
        return lastIndex();
    }

    uint64_t after = ents[0]->index - 1;
    CHECK_GE(after, mCommitted) << "after is out of range [committed]";

    mUnstable->truncateAndAppend(ents);
    return lastIndex();
}

uint64_t RaftLog::firstIndex()
{
    uint64_t i = 0;
    bool ret = mUnstable->maybeFirstIndex(i);
    if (ret)
    {
        return i;
    }
    i = mStorage->FirstIndex();
    assert(i > 0);
    return i;
}

uint64_t RaftLog::lastIndex()
{
    uint64_t i = 0;
    bool ret = mUnstable->maybeLastIndex(i);
    if (ret)
    {
        return i;
    }
    i = mStorage->LastIndex();
    assert(i != 0);
    return i;
}

bool RaftLog::isUpToDate(uint64_t i, uint64_t t)
{
    return t > lastTerm() || (t == lastTerm() && i >= lastIndex());
}

uint64_t RaftLog::lastTerm()
{
    uint64_t t = 0;
    term(lastIndex(), t);
    assert(t > 0);
    return t;
}


Status RaftLog::entries(uint64_t i, uint64_t maxSize, std::vector<LogEntry*>& ents)
{
    if (i > lastIndex())
    {
        return OK;
    }
    Status s = slice(i, lastIndex() + 1, maxSize, ents);
    return s;
}

// l.firstIndex <= lo <= hi <= l.firstIndex + len(l.entries)
Status RaftLog::mustCheckOutOfBounds(uint64_t low, uint64_t high)
{
    if (low > high)
    {
        assert(0);
    }

    uint64_t first = firstIndex();
    if (low < first)
    {
        return ERROR_COMPACTED;
    }

    uint64_t length = lastIndex() + 1 - first;
    if (high > first + length)
    {
        assert(0);
    }

    return OK;
}

Status RaftLog::slice(uint64_t low, uint64_t high, uint64_t pageSize, std::vector<LogEntry*>& ents)
{
    Status s = mustCheckOutOfBounds(low, high);
    if (!s.IsOK())
    {
        return s;
    }
    if (low == high)
    {
        return OK;
    }
    if (low >= mUnstable->mOffset)
    {
        std::vector<LogEntry*> entries = limitSize(mUnstable->slice(low, high), pageSize);
        ents.insert(ents.begin(), entries.begin(), entries.end());
        return OK;
    }
    uint64_t cut = std::min(high, mUnstable->mOffset);
    s = mStorage->Entries(low, cut, pageSize, ents);
    if (s == ERROR_COMPACTED)
    {
        return s;
    }
    else if (s == ERROR_UNAVAILABLE)
    {
        assert(0);
    }
    else if (!s.IsOK())
    {
        assert(0);
    }
    if (high <= mUnstable->mOffset)
    {
        return OK;
    }

    if (ents.size() < cut - low)
    {
        return OK;
    }

    uint64_t size = entsSize(ents);
    if (size >= pageSize)
    {
        return OK;
    }

    std::vector<LogEntry*> unstable = limitSize(mUnstable->slice(mUnstable->mOffset, high), pageSize - size);

    if (unstable.size() == 1 && size + entsSize(unstable) > pageSize)
    {
        return OK;
    }

    ents = extent(ents, unstable);

    return OK;
}

bool RaftLog::maybeCommit(uint64_t maxIndex, uint64_t term)
{
    uint64_t t = 0;
    this->term(maxIndex, t);
    if (maxIndex > mCommitted && term != 0 && zeroTermOnOutOfBounds(t) == term)
    {
        commitTo(maxIndex);
        return true;
    }
    return false;
}

void RaftLog::appliedTo(uint64_t i, uint64_t size)
{
    if (mCommitted < i || i < mApplied)
    {
        assert(0);
    }

    mApplied = i;
    mApplying = std::max(mApplying, i);

    if (mApplyingEntsSize > size)
    {
        mApplyingEntsSize -= size;
    }
    else
    {
        mApplyingEntsSize = 0;
    }
    mApplyingEntsPaused = mApplyingEntsSize >= mMaxApplyingEntsSize;
}


std::vector<LogEntry*> RaftLog::nextUnstableEnts()
{
    return mUnstable->nextEntries();
}

void RaftLog::stableTo(uint64_t i, uint64_t t)
{
    mUnstable->stableTo(i, t);
}


void RaftLog::acceptApplying(uint64_t i, uint64_t size, bool allowUnstable)
{
    if (mCommitted < i)
    {
        assert(0);
    }

    mApplying = i;
    mApplyingEntsSize += size;
    // Determine whether to pause entry application until some progress is
	// acknowledged. We pause in two cases:
	// 1. the outstanding entry size equals or exceeds the maximum size.
	// 2. the outstanding entry size does not equal or exceed the maximum size,
	//    but we determine that the next entry in the log will push us over the
	//    limit. We determine this by comparing the last entry returned from
	//    raftLog.nextCommittedEnts to the maximum entry that the method was
	//    allowed to return had there been no size limit. If these indexes are
	//    not equal, then the returned entries slice must have been truncated to
	//    adhere to the memory limit.
    mApplyingEntsPaused = mApplyingEntsSize >= mMaxApplyingEntsSize ||
        i < maxAppliableIndex(allowUnstable);
}

uint64_t RaftLog::maxAppliableIndex(bool allowUnstable)
{
    uint64_t high = mCommitted;
    if (!allowUnstable)
    {
        high = std::min(high, mUnstable->mOffset - 1);
    }
    return high;
}

void RaftLog::restore(const raft::Snapshot& snapshot)
{
    LOG(INFO) << "log [%s] starts to restore snapshot [index: " <<
        snapshot.meta().index() << ", term: " << snapshot.meta().term() <<"]";
    mCommitted = snapshot.meta().index();
    mSnapshot = snapshot;
    mUnstable->restore(&mSnapshot);
}

bool RaftLog::hasNextCommittedEnts(bool allowUnstable)
{
    if (mApplyingEntsPaused)
    {
        return false;
    }

    if (hasNextOrInProgressSnapshot())
    {
        return false;
    }

    return (mApplying + 1) < maxAppliableIndex(allowUnstable) + 1;
}

bool RaftLog::hasNextOrInProgressSnapshot()
{
    return mUnstable->mSnapshot != NULL;
}


std::vector<LogEntry*> RaftLog::nextCommittedEnts(bool allowUnstable)
{
    if (mApplyingEntsPaused)
    {
        return {};
    }

    if (hasNextOrInProgressSnapshot())
    {
        return {};
    }

    uint64_t low = mApplying + 1;
    uint64_t high = maxAppliableIndex(allowUnstable) + 1;
    if (low >= high)
    {
        return {};
    }

    uint64_t maxSize = mMaxApplyingEntsSize - mApplyingEntsSize;
    if (maxSize <= 0)
    {
        assert(0);
    }

    std::vector<LogEntry*> ents;
    Status s = slice(low, high, maxSize, ents);
    if (!s.IsOK())
    {
        assert(0);
    }
    return ents;
}


std::vector<LogEntry*> RaftLog::allEntries()
{
    std::vector<LogEntry*> ents;
    Status s = entries(firstIndex(), UINT64_MAX, ents);
    if (s.IsOK())
    {
        return ents;
    }
    if (s.Code() == ERROR_COMPACTED.Code())
    {
        return allEntries();
    }
    assert(0);
}


Status RaftLog::scan(uint64_t low, uint64_t high, uint64_t pageSize, std::function<Status(std::vector<LogEntry*>)> cb)
{
    while (low < high)
    {
        std::vector<LogEntry*> ents;
        Status s = slice(low, high, pageSize, ents);
    }
    return OK;
}

}