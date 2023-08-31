#include "gtest/gtest.h"
#include "status.h"
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

    void addEntry(RaftUnstable* unstable, uint64_t term, uint64_t index)
    {
        raft::LogEntry* log = new LogEntry { index, term, NULL };
        unstable->mEntries.push_back(log);
    }

    raft::LogEntry* makeEntry(uint64_t index, uint64_t term)
    {
        raft::LogEntry* log = new raft::LogEntry {index, term, NULL};
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
        uint64_t wantTerm = log->term(p.first);
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
        std::vector<uint64_t>   ents;
        uint64_t                windex;
        std::vector<uint64_t>   wents;
        uint64_t                wunstable;
    };

    void runAppend(AppendTestCase* t)
    {
        std::vector<raft::LogEntry*> previouseEnts;
        previouseEnts.push_back(makeEntry(1, 1));
        previouseEnts.push_back(makeEntry(2, 2));
        std::unique_ptr<MemoryStorage> stor = std::make_unique<MemoryStorage>();
        stor->Append(previouseEnts);

        std::unique_ptr<RaftLog> log = std::make_unique<RaftLog>(stor.release(), 4096);

        std::vector<raft::LogEntry*> ents;
        for (uint32_t i = 0 ;i < t->ents.size();)
        {
            ents.push_back(makeEntry(t->ents[i], t->ents[i+1]));
            i += 2;
        }
        EXPECT_EQ(t->windex, log->append(ents));

        std::vector<raft::LogEntry*> g = log->entries(1);
        EXPECT_EQ(t->wunstable, log->mUnstable->mOffset);
    }

protected:
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
        { {}, 2, {1, 1, 2, 2}, 3 },
        { {3, 2}, 3, {1, 1, 2, 2, 3, 2}, 3 },
        { {1, 2}, 1, {1, 2}, 1 },
        { {2, 3, 3, 3}, 3, {1, 1, 2, 3, 3, 3}, 2 },
    };

    for (uint32_t i = 0; i < cases.size(); i++)
    {
        runAppend(&cases[i]);
    }
}