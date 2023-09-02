#include "raft_log.h"
#include "status.h"

#include <butil/logging.h>
#include <vector>

namespace raft
{

RaftLog::RaftLog(Storage* stor, uint64_t maxApplyingEntsSize)
:   mMaxApplyingEntsSize(maxApplyingEntsSize)
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
    uint64_t t = this->term(i);
    if (t == 0)
    {
        return false;
    }
    return t == term;
}

uint64_t RaftLog::term(uint64_t i)
{
    uint64_t term = mUnstable->maybeTerm(i);
    if (term > 0)
    {
        return term;
    }
    if (i+1 < firstIndex())
    {
        return 0;
    }

    if (i > lastIndex())
    {
        return 0;
    }

    term = mStorage->Term(i);
    return term;

    assert(0);
}

uint64_t RaftLog::findConflict(std::vector<LogEntry*> ents)
{
    for (uint32_t i = 0; i < ents.size(); i++)
    {
        LogEntry* entry = ents[i];
        if (matchTerm(entry->index, entry->term) == 0)
        {
            if (entry->index <= lastIndex())
            {
                LOG(INFO) << "found conflict at index " << entry->index << " [existing term: " << zeroTermOnOutOfBounds(term(entry->index))
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
        uint64_t t = this->term(index);
        if (t == 0)
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
uint64_t RaftLog::maybeAppend(uint64_t index, uint64_t term, uint64_t committed, std::vector<LogEntry*> ents)
{
    if (!matchTerm(index, term))
    {
        return 0;
    }

    uint64_t lastnewi = index + ents.size();
    uint64_t conflict = findConflict(ents);
    if (conflict == 0 || conflict <= mCommitted)
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

    commitTo(std::min(mCommitted, lastnewi));

    return lastnewi;
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
    uint64_t i = mUnstable->maybeFirstIndex();
    if (i > 0)
    {
        return 0;
    }
    i = mStorage->FirstIndex();
    assert(i > 0);
    return i;
}

uint64_t RaftLog::lastIndex()
{
    uint64_t i = mUnstable->maybeLastIndex();
    if (i > 0)
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
    uint64_t t = term(lastIndex());
    assert(t > 0);
    return t;
}


std::vector<LogEntry*> RaftLog::entries(uint64_t i)
{
    if (i > lastIndex())
    {
        return {};
    }
    return {};
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

Status RaftLog::slice(uint64_t low, uint64_t high, std::vector<LogEntry*>& ents)
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
        std::vector<LogEntry*> entries = mUnstable->slice(low, high);
        for (uint i = 0; i < entries.size(); i++)
        {
            ents.push_back(entries[i]);
        }
        return OK;
    }
    uint64_t cut = std::min(high, mUnstable->mOffset);
    std::vector<LogEntry*> entries;
    s = mStorage->Entries(low, high, entries);
    if (s == ERROR_COMPACTED)
    {
        return s;
    }
    return OK;
}

}