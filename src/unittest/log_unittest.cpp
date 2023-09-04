#include "gtest/gtest.h"
#include "status.h"
#include "util.h"
#include "unittest/raft_unittest_util.h"
#include "raft_log.h"
#include "memory_storage.h"
#include <memory>

using namespace raft;

class LogFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
    }

    void TearDown()
    {
    }

    raft::LogEntry* makeEntry(uint64_t index, uint64_t term)
    {
        raft::LogEntry* log = new raft::LogEntry {index, term, NULL, 0};
        return log;
    }

    void addSnapshot(RaftUnstable* unstable, uint64_t term, uint64_t index)
    {
        raft::Snapshot* snapshot = new raft::Snapshot;
        raft::SnapshotMetadata* meta = new raft::SnapshotMetadata;
        meta->set_term(term);
        meta->set_index(index);
        snapshot->set_allocated_meta(meta);
        unstable->mSnapshot = snapshot;
    }

    struct FindConflictTestCase
    {
        std::vector<uint64_t> previousEnts;
        std::vector<uint64_t> ents;
        uint64_t wconflict;
    };

    void runFindConflict(FindConflictTestCase* t)
    {
        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(new MemoryStorage, 4096);
        std::vector<raft::LogEntry*> toappend;
        for (uint32_t i = 0; i < t->previousEnts.size();)
        {
            toappend.push_back(makeEntry(t->previousEnts[i], t->previousEnts[i+1]));
            i+=2;
        }
        log->append(toappend);

        std::vector<raft::LogEntry*> ents;
        for (uint32_t i = 0; i < t->ents.size();)
        {
            ents.push_back(makeEntry(t->ents[i], t->ents[i+1]));
            i += 2;
        }

        EXPECT_EQ(t->wconflict, log->findConflict(ents));
    }

    struct FindConflictByTermTestCase
    {
        std::vector<uint64_t> ents;
        uint64_t index;
        uint64_t term;
        uint64_t want;
    };

    void runFindConflictByTerm(FindConflictByTermTestCase* t)
    {
        std::unique_ptr<MemoryStorage> mem = std::make_unique<MemoryStorage>();
        raft::Snapshot snapshot;
        raft::SnapshotMetadata* meta = snapshot.mutable_meta();
        meta->set_index(t->ents[0]);
        meta->set_term(t->ents[1]);
        mem->ApplySnapshot(snapshot);

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(mem.release(), 4096);

        std::vector<raft::LogEntry*> ents;
        for (uint32_t i = 2; i < t->ents.size();)
        {
            ents.push_back(makeEntry(t->ents[i], t->ents[i+1]));
            i += 2;
        }
        log->append(ents);

        std::pair<uint64_t, uint64_t> p = log->findConflictByTerm(t->index, t->term);
        EXPECT_EQ(t->want, p.first);
        uint64_t wantTerm;
        log->term(p.first, wantTerm);
        wantTerm = log->zeroTermOnOutOfBounds(wantTerm);
        EXPECT_EQ(wantTerm, p.second);
    }

    struct IsUpToDateTestCase
    {
        int                   lastIndex;
        uint64_t              term;
        bool                  wUpToDate;
    };

    void runIsUpToDate(IsUpToDateTestCase* t)
    {
        std::vector<raft::LogEntry*> toappend;
        toappend.push_back(makeEntry(1, 1));
        toappend.push_back(makeEntry(2, 2));
        toappend.push_back(makeEntry(3, 3));
        std::unique_ptr<MemoryStorage> mem = std::make_unique<MemoryStorage>();
        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(mem.release(), 4096);

        log->append(toappend);
        
        EXPECT_EQ(t->wUpToDate, log->isUpToDate(log->lastIndex() + t->lastIndex, t->term));
    }

    struct AppendTestCase
    {
        std::vector<LogEntry>   ents;
        uint64_t                windex;
        std::vector<LogEntry>   wents;
        uint64_t                wunstable;
    };

    void runAppend(AppendTestCase* t)
    {
        std::vector<raft::LogEntry*> previouseEnts;
        previouseEnts.push_back(new LogEntry {1, 1});
        previouseEnts.push_back(new LogEntry {2, 2});
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        stor->Append(previouseEnts);

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), 4096);

        std::vector<raft::LogEntry*> ents;
        for (uint32_t i = 0 ;i < t->ents.size(); i++)
        {
            ents.push_back(&t->ents[i]);
        }
        EXPECT_EQ(t->windex, log->append(ents));

        std::vector<raft::LogEntry*> g;
        log->entries(1, UINT64_MAX, g);
        EXPECT_EQ(t->wunstable, log->mUnstable->mOffset);
    }

    struct MaybeAppendTestCase
    {
        uint64_t logTerm;
        uint64_t index;
        uint64_t committed;
        std::vector<LogEntry> ents;

        uint64_t wlasti;
        bool     wappend;
        uint64_t wcommit;
        bool     wpanic;
    };

    void runMaybeAppend(MaybeAppendTestCase* t)
    {
        std::vector<LogEntry*> prevEnts;
        prevEnts.push_back(new LogEntry(1, 1));
        prevEnts.push_back(new LogEntry(2, 2));
        prevEnts.push_back(new LogEntry(3, 3));

        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        stor->Append(prevEnts);
        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), UINT64_MAX);
        log->mCommitted = 1;
        std::vector<LogEntry*> append;
        for (uint i = 0; i < t->ents.size(); i++)
        {
            append.push_back(&t->ents[i]);
        }

        uint64_t glasti = 0;
        bool appendSucc = log->maybeAppend(t->index, t->logTerm, t->committed, append, glasti);
        EXPECT_EQ(t->wlasti, glasti);
        EXPECT_EQ(t->wappend, appendSucc);
        EXPECT_EQ(t->wcommit, log->mCommitted);
        if (appendSucc && !t->ents.empty())
        {
            std::vector<LogEntry*> gents;
            Status s = log->slice(log->lastIndex() - t->ents.size() + 1, log->lastIndex() + 1, UINT64_MAX, gents);
            EXPECT_TRUE(s.IsOK());
            EXPECT_EQ(gents.size(), t->ents.size());
        }
    }

};

TEST_F(LogFixture, FindConflict)
{
    std::vector<LogFixture::FindConflictTestCase> cases = {
        // no conflict, empty ent
        { {1, 1, 2, 2, 3, 3}, {}, 0 },
        // no conflict
        { {1, 1, 2, 2, 3, 3}, {1, 1, 2, 2, 3, 3}, 0},
        { {1, 1, 2, 2, 3, 3}, {2, 2, 3, 3}, 0 },
        { {1, 1, 2, 2, 3, 3}, {3, 3}, 0 },
        // no conflict, but has new entries
        { {1, 1, 2, 2, 3, 3}, {1, 1, 2, 2, 3, 3, 4, 4, 5, 4}, 4 },
        { {1, 1, 2, 2, 3, 3}, {2, 2, 3, 3, 4, 4, 5, 4}, 4 },
        { {1, 1, 2, 2, 3, 3}, {3, 3, 4, 4, 5, 4}, 4 },
        { {1, 1, 2, 2, 3, 3}, {4, 4, 5, 4}, 4 },
        // conflicts with existing entries
        { {1, 1, 2, 2, 3, 3}, {1, 4, 2, 4}, 1 },
        { {1, 1, 2, 2, 3, 3}, {2, 1, 3, 4, 4, 4}, 2 },
        { {1, 1, 2, 2, 3, 3}, {3, 1, 4, 2, 5, 4, 6, 4}, 3 }
    };

    for (uint32_t i = 0; i < cases.size(); i++)
    {
        runFindConflict(&cases[i]);
    }
}

TEST_F(LogFixture, FindConflictByTerm)
{
    std::vector<LogFixture::FindConflictByTermTestCase> cases = {
        // Log starts from index 1.
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 100, 2, 100 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 5, 6, 5 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 5, 5, 5 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 5, 4, 2 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 5, 2, 2 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 5, 1, 0 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 1, 2, 1 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 1, 1, 0 },
        { { 0, 0, 1, 2, 2, 2, 3, 5, 4, 5, 5, 5 }, 0, 0, 0 },
        // Log with compacted entries.
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 30, 3, 30 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 14, 9, 14 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 14, 4, 14 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 14, 3, 12 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 14, 2, 9 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 11, 5, 11 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 10, 5, 10 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 10, 3, 10 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 10, 2, 9 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 9, 2, 9 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 4, 2, 4 },
        { { 10, 3, 11, 3, 12, 3, 13, 4, 14, 4, 15, 4 }, 0, 0, 0 },
    };

    for (uint32_t i = 0; i < cases.size(); i++)
    {
        runFindConflictByTerm(&cases[i]);
    }
}

TEST_F(LogFixture, IsUpToDateTestCase)
{
    std::vector<LogFixture::IsUpToDateTestCase> cases = {
        // greater term, ignore lastIndex
        { -1, 4, true },
        { 0, 4, true },
        { 1, 4, true },
        // smaller term, ignore lastIndex
        { -1, 2, false },
        { 0, 2, false },
        { 1, 2, false },
        // equal term, equal or lager lastIndex wins
        { -1, 3, false },
        { 0, 3, true },
        { 1, 3, true },
    };

    for (uint32_t i = 0; i < cases.size(); i++)
    {
        runIsUpToDate(&cases[i]);
    }
}

TEST_F(LogFixture, Append)
{
    std::vector<LogFixture::AppendTestCase> cases = {
        { {}, 2, { {1, 1}, {2, 2} }, 3 },
        { { {3, 2} }, 3, { {1, 1}, {2, 2}, {3, 2} }, 3 },
        { { {1, 2} }, 1, { {1, 2} }, 1 },
        { { {2, 3}, {3, 3} }, 3, { {1, 1}, {2, 3}, {3, 3} }, 2 },
    };

    FOREACH(iter, cases)
    {
        runAppend(&*iter);
    }
}

TEST_F(LogFixture, LogMaybeAppend)
{
    uint64_t lastindex = 3;
    uint64_t lastterm = 3;
    uint64_t commit = 1;

    std::vector<MaybeAppendTestCase> cases = {
        // not match: term is different
        { lastterm - 1, lastindex, lastindex, { {lastindex + 1, 4} }, 0, false, commit, false },
        // not match: index out of bound
        { lastterm, lastindex + 1, lastindex, { {lastindex + 2, 4} }, 0, false, commit, false },
        // match with the last existing entry
        { lastterm, lastindex, lastindex, { }, lastindex, true, lastindex, false },
        { lastterm, lastindex, lastindex + 1, { }, lastindex, true, lastindex, false },
        { lastterm, lastindex, lastindex - 1, { }, lastindex, true, lastindex - 1, false },
        { lastterm, lastindex, 0, { }, lastindex, true, commit, false },
        { 0, 0, lastindex, { }, 0, true, commit, false },
        // { lastterm, lastindex, lastindex, { {lastindex + 1, 4} }, lastindex + 1, true, lastindex, false },
        // { lastterm, lastindex, lastindex + 1, { {lastindex + 1, 4} }, lastindex + 1, true, lastindex + 1, false },
        // { lastterm, lastindex, lastindex + 2, { {lastindex + 1, 4} }, lastindex + 1, true, lastindex + 1, false },
        // { lastterm, lastindex, lastindex + 2, { {lastindex + 1, 4}, { lastindex + 2, 4} }, lastindex + 2, true, lastindex + 2, false },
        // match with the entry in the middle
        // { lastterm - 1, lastindex - 1, lastindex, { { lastindex, 4 } }, lastindex, true, lastindex, false },
        // { lastterm - 2, lastindex - 2, lastindex, { { lastindex - 1, 4 } }, lastindex - 1, true, lastindex - 1, false },
        // // { lastterm - 3, lastindex - 3, lastindex, { { lastindex - 2, 4 } }, lastindex - 2, true, lastindex - 2, true },
        // { lastterm - 2, lastindex - 2, lastindex, { { lastindex - 1, 4 }, { lastindex, 4 } }, lastindex, true, lastindex, false },
    };

    FOREACH(iter, cases)
    {
        runMaybeAppend(&*iter);
    }
}

TEST_F(LogFixture, CompactionSideEffects)
{
    uint64_t i;
    uint64_t lastIndex = 1000;
    uint64_t unstableIndex = 750;
    uint64_t lastTerm = lastIndex;
    std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
    for (uint i = 1; i <= unstableIndex; i++)
    {
        stor->Append({ new LogEntry(i, i) });
    }

    std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.get(), 0);
    for (i = unstableIndex; i < lastIndex; i++)
    {
        log->append({ new LogEntry(i + 1, i + 1) });
    }

    EXPECT_TRUE(log->maybeCommit(lastIndex, lastTerm));
    log->appliedTo(log->mCommitted, 0);
    
    uint64_t offset = 500;
    stor->Compact(offset);
    EXPECT_EQ(lastIndex, log->lastIndex());
    
    for (uint j = offset; j <= log->lastIndex(); j++)
    {
        uint64_t t;
        Status s = log->term(j, t);
        EXPECT_EQ(j, t);
    }

    for (uint j = offset; j <= log->lastIndex(); j++)
    {
        EXPECT_TRUE(log->matchTerm(j, j));
    }

    std::vector<LogEntry*> unstableEnts = log->nextUnstableEnts();
    EXPECT_EQ(unstableEnts.size(), 250);
    EXPECT_EQ(unstableEnts[0]->index, 751);

    uint64_t prev = log->lastIndex();
    log->append({new LogEntry { log->lastIndex() + 1, log->lastIndex() + 1 }});
    EXPECT_EQ(prev + 1, log->lastIndex());

    std::vector<LogEntry*> ents;
    log->entries(log->lastIndex(), UINT64_MAX, ents);
    EXPECT_EQ(1, ents.size());
    stor.release();
}

TEST_F(LogFixture, HasNextCommittedEnts)
{
    raft::Snapshot snapshot;
    raft::SnapshotMetadata* meta = snapshot.mutable_meta();
    meta->set_index(3);
    meta->set_term(1);

    struct HasNextCommittedEntsCase
    {
        uint64_t applied;
        uint64_t applying;
        bool     allowUnstable;
        bool     paused;
        bool     snap;
        bool     whasNext;
    };

    std::vector<HasNextCommittedEntsCase> cases = {
        { 3, 3, true, 0, 0, true },
        { 3, 4, true, 0, 0, true },
        { 3, 5, true, 0, 0, false },
        { 4, 4, true, 0, 0, true },
        { 4, 5, true, 0, 0, false },
        { 5, 5, true, 0, 0, false },
        // Don't allow unstable entries.
        { 3, 3, false, 0, 0, true },
        { 3, 4, false, 0, 0, false },
        { 3, 5, false, 0, 0, false },
        { 4, 4, false, 0, 0, false },
        { 4, 5, false, 0, 0, false },
        { 5, 5, false, 0, 0, false },
        // Paused.
        { 3, 3, true, true, 0, false},
        // With snapshot.
        { 3, 3, true, 0, true, false},
    };

    FOREACH(iter, cases)
    {
        std::vector<LogEntry*> ents = { new LogEntry {4, 1}, new LogEntry {5, 1}, new LogEntry {6, 1}};
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        stor->ApplySnapshot(snapshot);
        std::vector<LogEntry*> toappend;
        toappend.insert(toappend.begin(), ents.begin(), ents.begin() + 1);
        stor->Append(toappend);

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), 0);
        log->append(ents);
        log->stableTo(4, 1);
        log->maybeCommit(5, 1);
        log->appliedTo(iter->applied, 0);
        log->acceptApplying(iter->applying, 0, iter->allowUnstable);
        log->mApplyingEntsPaused = iter->paused;

        if (iter->snap)
        {
            raft::Snapshot newSnapshot = snapshot;
            newSnapshot.mutable_meta()->set_index(newSnapshot.meta().index() + 1);
            log->restore(newSnapshot);
        }
        EXPECT_EQ(iter->whasNext, log->hasNextCommittedEnts(iter->allowUnstable));
    }
}

TEST_F(LogFixture, NextCommittedEnts)
{
    raft::Snapshot snapshot;
    raft::SnapshotMetadata* meta = snapshot.mutable_meta();
    meta->set_index(3);
    meta->set_term(1);

    struct NextCommittedEntsCase
    {
        uint64_t    applied;
        uint64_t    applying;
        bool        allowUnstable;
        bool        paused;
        bool        snap;
        std::vector<LogEntry>  wents;
    };

    std::vector<NextCommittedEntsCase> cases = {
        { 3, 3, true, 0, 0, { {4, 1}, { 5, 1 } } },
        { 3, 4, true, 0, 0, { { 5, 1 } } },
        { 3, 5, true, 0, 0, { } },
        { 4, 4, true, 0, 0, { { 5, 1 } } },
        { 4, 5, true, 0, 0, { } },
        { 5, 5, true, 0, 0, { } },
        // Don't allow unstable entries.
        { 3, 3, false, 0, 0, { { 4, 1 } } },
        { 3, 4, false, 0, 0, { } },
        { 3, 5, false, 0, 0, { } },
        { 4, 4, false, 0, 0, { } },
        { 4, 5, false, 0, 0, { } },
        { 5, 5, false, 0, 0, { } },
        // Paused.
        { 3, 3, true, true, 0, { } },
        // With snapshot.
        { 3, 3, true, true, true, { } },
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        stor->ApplySnapshot(snapshot);
        stor->Append({ new LogEntry{4, 1}});

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), UINT64_MAX);
        log->append({ new LogEntry {4, 1}, new LogEntry {5, 1}, new LogEntry {6, 1} });
        log->stableTo(4, 1);
        log->maybeCommit(5, 1);
        log->appliedTo(iter->applied, 0);
        log->acceptApplying(iter->applying, 0, iter->allowUnstable);
        log->mApplyingEntsPaused = iter->paused;
        if (iter->snap)
        {
            raft::Snapshot newSnapshot = snapshot;
            newSnapshot.mutable_meta()->set_index(snapshot.meta().index());
            log->restore(newSnapshot);
        }
        std::vector<LogEntry*> next = log->nextCommittedEnts(iter->allowUnstable);
        EXPECT_EQ(next.size(), iter->wents.size());
        for (uint i = 0; i < iter->wents.size(); i++)
        {
            EXPECT_EQ(next[i]->index, iter->wents[i].index);
            EXPECT_EQ(next[i]->term, iter->wents[i].term);
        }
    }
}

TEST_F(LogFixture, AcceptApplying)
{
    uint64_t maxSize = 100;
    raft::Snapshot snapshot;
    snapshot.mutable_meta()->set_index(3);
    snapshot.mutable_meta()->set_term(1);

    struct AcceptApplyingCase
    {
        uint64_t    index;
        bool        allowUnstable;
        uint64_t    size;
        bool        wpaused;
    };

    std::vector<AcceptApplyingCase> cases = {
        { 3, true, maxSize - 1, true},
        { 3, true, maxSize, true},
        { 3, true, maxSize + 1, true},
        { 4, true, maxSize - 1, true},
        { 4, true, maxSize, true},
        { 4, true, maxSize + 1, true},
        { 5, true, maxSize - 1, false},
        { 5, true, maxSize, true},
        { 5, true, maxSize + 1, true},
        // Don't allow unstable entries.
        { 3, false, maxSize - 1, true},
        { 3, false, maxSize, true},
        { 3, false, maxSize + 1, true},
        { 4, false, maxSize - 1, false},
        { 4, false, maxSize, true},
        { 4, false, maxSize + 1, true},
        { 5, false, maxSize - 1, false},
        { 5, false, maxSize, true},
        { 5, false, maxSize + 1, true},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        stor->ApplySnapshot(snapshot);
        stor->Append({new LogEntry {4, 1}});

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), maxSize);
        log->append({ new LogEntry { 4, 1}, new LogEntry {5, 1}, new LogEntry {6, 1}});
        log->stableTo(4, 1);
        log->maybeCommit(5, 1);
        log->appliedTo(3, 0);
        log->acceptApplying(iter->index, iter->size, iter->allowUnstable);
        EXPECT_EQ(iter->wpaused, log->mApplyingEntsPaused);
    }
}

TEST_F(LogFixture, AppliedTo)
{
    uint64_t maxSize = 100;
    uint64_t overshoot = 5;
    raft::Snapshot snapshot;
    snapshot.mutable_meta()->set_index(3);
    snapshot.mutable_meta()->set_term(1);

    struct AppliedToCase
    {
        uint64_t    index;
        uint64_t    size;
        uint64_t    wapplyingSize;
        bool        wpaused;
    };

    std::vector<AppliedToCase> cases = {
        // Apply some of in-progress entries (applying = 5 below).
        { 4, overshoot - 1, maxSize + 1, true},
        { 4, overshoot, maxSize, true},
        { 4, overshoot + 1, maxSize - 1, false},
        // Apply all of in-progress entries.
        { 5, overshoot - 1, maxSize + 1, true},
        { 5, overshoot, maxSize, true},
        { 5, overshoot + 1, maxSize - 1, false},
        // Apply all of outstanding bytes.
        { 4, maxSize + overshoot, 0, false},
        // Apply more than outstanding bytes.
		// Incorrect accounting doesn't underflow applyingSize.
        { 4, maxSize + overshoot + 1, 0, false},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        stor->ApplySnapshot(snapshot); 
        stor->Append({ new LogEntry { 4 , 1}});

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), maxSize);
        log->append({ new LogEntry { 4, 1}, new LogEntry {5, 1}, new LogEntry {6, 1}});
        log->stableTo(4, 1);
        log->maybeCommit(5, 1);
        log->appliedTo(3, 0);
        log->acceptApplying(5, maxSize + overshoot, false);

        log->appliedTo(iter->index, iter->size);
        EXPECT_EQ(iter->index, log->mApplied);
        EXPECT_EQ(5, log->mApplying);
        EXPECT_EQ(iter->wapplyingSize, log->mApplyingEntsSize);
        EXPECT_EQ(iter->wpaused, log->mApplyingEntsPaused);
    }
}

TEST_F(LogFixture, NextUnstableEnts)
{
    struct NextUnstableEntsCase
    {
        uint64_t                unstable;
        std::vector<LogEntry>   wents;
    };

    std::vector<NextUnstableEntsCase> cases = {
        { 3, {} },
        { 1, {{1, 1}, {2, 2}} }
    };

    std::vector<LogEntry> previousEnts = { {1, 1}, {2, 2} };

    FOREACH(iter, cases)
    {
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        std::vector<LogEntry*> toappend;
        for (uint i = 0; i < iter->unstable - 1; i++)
        {
            toappend.push_back(new LogEntry { previousEnts[i].index, previousEnts[i].term });
        }
        stor->Append(toappend);

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), UINT64_MAX);
        toappend.clear();
        for (uint i = iter->unstable - 1; i < previousEnts.size(); i++)
        {
            toappend.push_back(new LogEntry { previousEnts[i].index, previousEnts[i].term });
        }
        log->append(toappend);

        std::vector<LogEntry*> ents = log->nextUnstableEnts();
        if (ents.size() > 0)
        {
            log->stableTo(ents.back()->index, ents.back()->term);
        }

        EXPECT_EQ(ents.size(), iter->wents.size());
        for (uint i = 0; i < iter->wents.size(); i++)
        {
            EXPECT_EQ(ents[i]->index, iter->wents[i].index);
            EXPECT_EQ(ents[i]->term, iter->wents[i].term);
        }

        EXPECT_EQ(previousEnts.back().index + 1, log->mUnstable->mOffset);
    }
}

TEST_F(LogFixture, CommitTo)
{
    uint64_t commit = 2;
    struct CommitToCase
    {
        uint64_t    commit;
        uint64_t    wcommit;
        bool        wpanic;
    };

    std::vector<CommitToCase> cases = {
        { 3, 3, false},
        { 1, 2, false}, // never decrease
        // { 4, 0, true},  // commit out of range -> panic
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(new MemoryStorage, UINT64_MAX);
        log->append({ new LogEntry { 1, 1}, new LogEntry { 2, 2}, new LogEntry { 3, 3}});
        log->mCommitted = commit;
        log->commitTo(iter->commit);
        EXPECT_EQ(iter->wcommit, log->mCommitted);
    }
}

TEST_F(LogFixture, StableTo)
{
    struct StableToCase
    {
        uint64_t    stablei;
        uint64_t    stablet;
        uint64_t    wunstable;
    };

    std::vector<StableToCase> cases = {
        { 1, 1, 2},
        { 2, 2, 3},
        { 2, 1, 1},
        { 3, 1, 1},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(new MemoryStorage, UINT64_MAX);
        log->append({ new LogEntry {1, 1}, new LogEntry {2, 2}});
        log->stableTo(iter->stablei, iter->stablet);
        EXPECT_EQ(iter->wunstable, log->mUnstable->mOffset);
    }
}

TEST_F(LogFixture, StableToWithSnap)
{
    uint64_t snapi = 5;
    uint64_t snapt = 2;
    struct StableToWithSnapCase
    {
        uint64_t    stablei;
        uint64_t    stablet;
        std::vector<LogEntry*>   newEnts;
        uint64_t    wunstable;
    };

    std::vector<StableToWithSnapCase> cases = {
        { snapi + 1, snapt, { }, snapi + 1 },
        { snapi, snapt, { }, snapi + 1 },
        { snapi - 1, snapt, { }, snapi + 1 },

        { snapi + 1, snapt + 1, { }, snapi + 1 },
        { snapi, snapt + 1, { }, snapi + 1 },
        { snapi - 1, snapt + 1, { }, snapi + 1 },

        { snapi + 1, snapt, { new LogEntry {snapi + 1, snapt} }, snapi + 2 },
        { snapi, snapt, { new LogEntry {snapi + 1, snapt} }, snapi + 1 },
        { snapi - 1, snapt, { new LogEntry {snapi + 1, snapt} }, snapi + 1 },

        { snapi + 1, snapt + 1, { new LogEntry {snapi + 1, snapt} }, snapi + 1 },
        { snapi, snapt + 1, { new LogEntry {snapi + 1, snapt} }, snapi + 1 },
        { snapi - 1, snapt + 1, { new LogEntry {snapi + 1, snapt} }, snapi + 1 },
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        raft::Snapshot snapshot;
        snapshot.mutable_meta()->set_index(snapi);
        snapshot.mutable_meta()->set_term(snapt);
        stor->ApplySnapshot(snapshot);

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), UINT64_MAX);
        log->append(iter->newEnts);
        log->stableTo(iter->stablei, iter->stablet);
        EXPECT_EQ(iter->wunstable, log->mUnstable->mOffset);
    }
}

TEST_F(LogFixture, Compaction)
{
    struct CompactionCase
    {
        uint64_t                lastIndex;
        std::vector<uint64_t>   compact;
        std::vector<int>        wleft;
        bool                    wallow;
    };

    std::vector<CompactionCase> cases = {
        // out of upper bound
        // {1000, { 1001}, { -1}, false},
        {1000, { 300, 500, 800, 900}, { 700, 500, 200, 100}, true},
        // out of lower bound
        // {1000, { 300, 299}, { 700, -1}, false},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        for (uint i = 1; i <= iter->lastIndex; i++)
        {
            stor->Append({ new LogEntry { i, 0}});
        }

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.get(), UINT64_MAX);
        log->maybeCommit(iter->lastIndex, 0);
        log->appliedTo(log->mCommitted, 0);
        for (uint j = 0; j < iter->compact.size(); j++)
        {
            Status s = stor->Compact(iter->compact[j]);
            if (!s.IsOK())
            {
                EXPECT_FALSE(iter->wallow);
                continue;
            }
            EXPECT_EQ(iter->wleft[j], log->allEntries().size());
        }
        stor.release();
    }
}

TEST_F(LogFixture, LogRestore)
{
    uint64_t index = 1000;
    uint64_t term = 1000;
    raft::Snapshot snapshot;
    snapshot.mutable_meta()->set_index(index);
    snapshot.mutable_meta()->set_term(term);
    std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
    stor->ApplySnapshot(snapshot);

    std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.get(), UINT64_MAX);

    EXPECT_EQ(0, log->allEntries().size());
    EXPECT_EQ(index + 1, log->firstIndex());
    EXPECT_EQ(index, log->mCommitted);
    EXPECT_EQ(index + 1, log->mUnstable->mOffset);
    uint64_t t;
    log->term(index, t);
    EXPECT_EQ(term, t);

    stor.release();
}

TEST_F(LogFixture, Term)
{
    uint64_t offset = 100;
    uint64_t num = 100;

    struct TermCase
    {
        uint64_t    idx;
        uint64_t    term;
        Status      error;
    };

    std::vector<TermCase> cases = {
        { offset - 1, 0, ERROR_COMPACTED},
        { offset, 1, OK},
        { offset + num / 2, num / 2, OK},
        { offset + num - 1, num - 1, OK},
        { offset + num, 0, ERROR_UNAVAILABLE},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        raft::Snapshot snapshot;
        snapshot.mutable_meta()->set_index(offset);
        snapshot.mutable_meta()->set_term(1);
        stor->ApplySnapshot(snapshot);

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), UINT64_MAX);
        for (uint i = 1; i < num; i++)
        {
            log->append({ new LogEntry {offset + i, i}});
        }

        uint64_t term;
        Status s = log->term(iter->idx, term);
        EXPECT_EQ(iter->term, term);
        EXPECT_EQ(iter->error.Code(), s.Code());
    }
}

TEST_F(LogFixture, TermWithUnstableSnapshot)
{
    uint64_t storagesnapi = 100;
    uint64_t unstablesnapi = storagesnapi + 5;

    struct TermWithUnstableSnapshotCase
    {
        uint64_t    idx;
        uint64_t    term;
        Status      error;
    };

    std::vector<TermWithUnstableSnapshotCase> cases = {
        { storagesnapi, 0, ERROR_COMPACTED},
        { storagesnapi + 1, 0, ERROR_COMPACTED},
        { unstablesnapi - 1, 0, ERROR_COMPACTED},
        { unstablesnapi, 1, OK},
        { unstablesnapi + 1, 0, ERROR_UNAVAILABLE},
    };

    std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
    raft::Snapshot snapshot;
    snapshot.mutable_meta()->set_index(storagesnapi);
    snapshot.mutable_meta()->set_term(1);
    stor->ApplySnapshot(snapshot);

    std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), UINT64_MAX); 
    raft::Snapshot snapshot2;
    snapshot2.mutable_meta()->set_index(unstablesnapi);
    snapshot2.mutable_meta()->set_term(1);
    log->restore(snapshot2);

    FOREACH(iter, cases)
    {
        uint64_t term = 0;
        Status s = log->term(iter->idx, term);
        EXPECT_EQ(term, iter->term);
        EXPECT_EQ(s.Code(), iter->error.Code());
    }
}

TEST_F(LogFixture, Slice)
{
    uint64_t offset = 100;
    uint64_t num = 100;
    uint64_t last = offset + num;
    uint64_t half = offset + num / 2;
    LogEntry halfe = { half, half};

    std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>(); 
    raft::Snapshot snapshot;
    snapshot.mutable_meta()->set_index(offset);
    snapshot.mutable_meta()->set_term(0);
    stor->ApplySnapshot(snapshot);
    std::vector<LogEntry*> toappend;
    for (uint i = offset + 1; i < half; i++)
    {
        toappend.push_back(new LogEntry {i, i});
    }
    stor->Append(toappend);

    std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), UINT64_MAX);
    toappend.clear();
    for (uint i = half; i < last; i++)
    {
        toappend.push_back(new LogEntry {i, i});
    }
    log->append(toappend);

    struct SliceCase
    {
        uint64_t    low;
        uint64_t    high;
        uint64_t    limit;
        uint64_t    wstart;
        uint64_t    length;
        bool                    wpanic;
    };

    std::vector<SliceCase> cases = {
        // ErrCompacted.
        { offset - 1, offset + 1, UINT64_MAX, 0, 0, false},
        { offset, offset + 1, UINT64_MAX, 0, 0, false},
        // panics
        // { half, half - 1, UINT64_MAX, {}, true},
        // { half, half + 1, UINT64_MAX, {}, true},

        // No limit.
        { offset + 1, offset + 1, UINT64_MAX, 0, 0, false},
        { offset + 1, half - 1, UINT64_MAX, offset + 1, half - offset - 2, false},
        { offset + 1, half, UINT64_MAX, offset + 1, half - offset - 1, false},
        { offset + 1, half + 1, UINT64_MAX, offset + 1, half - offset, false},
        { offset + 1, last, UINT64_MAX, offset + 1, last - offset - 1, false},

        { half - 1, half, UINT64_MAX, half - 1, 1, false},
        { half - 1, half + 1, UINT64_MAX, half - 1, 2, false},
        { half - 1, last, UINT64_MAX, half - 1, last - half + 1, false},
        { half, half + 1, UINT64_MAX, half, 1, false},
        { half, last, UINT64_MAX, half, last - half, false},
        { last - 1, last, UINT64_MAX, last - 1, 1, false},

        // At least one entry is always returned.
        { offset + 1, last, 0, offset + 1, 1, false},
        { half - 1, half + 1, 0, half - 1, 1, false},
        { half, last, 0, half, 1, false},
        { half + 1, last, 0, half + 1, 1, false},
        // Low limit.
        { offset + 1, last, halfe.size() - 1, offset + 1, 1, false},
        { half - 1, half + 1, halfe.size() - 1, half - 1, 1, false},
        { half, last, halfe.size() - 1, half, 1, false},
        // Just enough for one limit.
        { offset + 1, last, halfe.size(), offset + 1, 1, false},
        { half - 1, half + 1, halfe.size(), half - 1, 1, false},
        { half, last, halfe.size(), half, 1, false},
        // Not enough for two limit.
        { offset + 1, last, halfe.size() + 1, offset + 1, 1, false},
        { half - 1, half + 1, halfe.size() + 1, half - 1, 1, false},
        { half, last, halfe.size() + 1, half, 1, false},
        // Enough for two limit.
        { offset + 1, last, halfe.size() * 2, offset + 1, 2, false},
        { half - 2, half + 1, halfe.size() * 2, half - 2, 2, false},
        { half - 1, half + 1, halfe.size() * 2, half - 1, 2, false},
        { half, last, halfe.size() * 2, half, 2, false},
        // Not enough for three.
        { half - 2, half + 1, halfe.size() * 3 - 1, half - 2, 2, false},
        // Enough for three.
        { half - 1, half + 2, halfe.size() * 3, half - 1, 3, false},
    };

    FOREACH(iter, cases)
    {
        std::vector<LogEntry*> g;
        Status s = log->slice(iter->low, iter->high, iter->limit, g);
        EXPECT_FALSE(iter->low <= offset && s.Code() != ERROR_COMPACTED.Code());
        EXPECT_FALSE(iter->low > offset && s.Code() != OK.Code());
        EXPECT_EQ(iter->length, g.size());
        for (uint i = 0; i < g.size(); i++)
        {
            EXPECT_EQ(g[i]->index, iter->wstart + i);
            EXPECT_EQ(g[i]->term, iter->wstart + i);
        }
    }
}

TEST_F(LogFixture, Scan)
{
    uint64_t offset = 47;
    uint64_t num = 20;
    uint64_t last = offset + num;
    uint64_t half = offset + num / 2;
}