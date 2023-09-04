#include "gtest/gtest.h"
#include "status.h"
#include "util.h"
#include "unittest/raft_unittest_util.h"
#include "raft_unstable_log.h"
#include <memory>

using namespace raft;

class UnstableFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        mUnstable.reset(new RaftUnstable);
    }

    void TearDown()
    {
        for (uint i = 0; i < mUnstable->mEntries.size(); i++)
        {
            delete mUnstable->mEntries[i];
        }
        mUnstable.reset(NULL);
    }

    void addEntry(RaftUnstable* unstable, uint64_t term, uint64_t index)
    {
        raft::LogEntry* log = new raft::LogEntry;
        log->index = index;
        log->term = term;
        unstable->mEntries.push_back(log);
    }

    raft::LogEntry* makeEntry(uint64_t index, uint64_t term)
    {
        raft::LogEntry* log = new raft::LogEntry;
        log->index = index;
        log->term = term;
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

protected:
    std::unique_ptr<RaftUnstable> mUnstable;
};

TEST_F(UnstableFixture, MaybeFirstIndex)
{
    struct MaybeFirstIndexCase
    {
        LogEntry*   add;
        LogEntry*   snapshot;
        int64_t     offset;
        uint64_t    windex;
    };

    std::vector<MaybeFirstIndexCase> cases = {
        { new LogEntry {5, 1}, NULL, 5, 0},
        { NULL, NULL, -1, 0},
        { new LogEntry {5, 1}, new LogEntry {4, 1}, 5, 5},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        if (iter->add)
        {
            unstable->mEntries.push_back(iter->add);
        }
        if (iter->offset > 0)
        {
            unstable->mOffset = iter->offset;
        }
        if (iter->snapshot)
        {
            addSnapshot(unstable.get(), iter->snapshot->term, iter->snapshot->index);
        }
        uint64_t i = 0;
        unstable->maybeFirstIndex(i);
        EXPECT_EQ(i, iter->windex);
    }
}

TEST_F(UnstableFixture, MaybeLastIndex)
{
    struct MaybeLastIndexCase
    {
        LogEntry*   add;
        LogEntry*   snapshot;
        uint64_t    offset;
        bool        wok;
        uint64_t    woffset;
    };

    std::vector<MaybeLastIndexCase> cases = {
        { new LogEntry { 5, 1}, NULL, 5, true, 5},
        { new LogEntry { 5, 1}, new LogEntry { 4, 1},  5, true, 5},
        { NULL, new LogEntry { 4, 1},  5, true, 4},
        { NULL, NULL, 0, false, 0},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        if (iter->add)
        {
            unstable->mEntries.push_back(iter->add);
        }

        if (iter->snapshot)
        {
            raft::Snapshot* snapshot = new raft::Snapshot;
            raft::SnapshotMetadata* meta = new raft::SnapshotMetadata;
            meta->set_term(iter->snapshot->term);
            meta->set_index(iter->snapshot->index);
            snapshot->set_allocated_meta(meta);
            unstable->mSnapshot = snapshot;
        }

        unstable->mOffset = iter->offset;
        uint64_t i = 0;
        bool isok = unstable->maybeLastIndex(i);
        EXPECT_EQ(isok, iter->wok);
        EXPECT_EQ(i, iter->woffset);
    }
}

TEST_F(UnstableFixture, MaybeTerm)
{
    struct MaybeTermCase
    {
        std::vector<LogEntry*>  entries;
        uint64_t                offset;
        LogEntry*               snapshot;
        uint64_t                index;
        bool                    wok;
        uint64_t                wterm;
    };

    std::vector<MaybeTermCase> cases = {
        // term from entries
        { { new LogEntry {5, 1} }, 5, NULL, 5, true, 1},
        { { new LogEntry {5, 1} }, 5, NULL, 6, false, 0},
        { { new LogEntry {5, 1} }, 5, NULL, 4, false, 0},
        { { new LogEntry {5, 1} }, 5, new LogEntry { 4, 1}, 5, true, 1},
        { { new LogEntry {5, 1} }, 5, new LogEntry { 4, 1}, 6, false, 0},
        // term from snapshot
        { { new LogEntry {5, 1} }, 5, new LogEntry { 4, 1}, 4, true, 1},
        { { new LogEntry {5, 1} }, 5, new LogEntry { 4, 1}, 3, false, 0},
        { { }, 5, new LogEntry { 4, 1}, 5, false, 0},
        { { }, 5, new LogEntry { 4, 1}, 4, true, 1},
        { { }, 0, NULL, 5, false, 0},
    };

    FOREACH(iter, cases)
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        unstable->mEntries = iter->entries;
        unstable->mOffset = iter->offset;
        if (iter->snapshot)
        {
            raft::Snapshot* snapshot = new raft::Snapshot;
            snapshot->mutable_meta()->set_term(iter->snapshot->term);
            snapshot->mutable_meta()->set_index(iter->snapshot->index);
            unstable->mSnapshot = snapshot;
        }
        uint64_t term = 0;
        bool isok = unstable->maybeTerm(iter->index, term);
        EXPECT_EQ(isok, iter->wok);
        EXPECT_EQ(term, iter->wterm);
    }
}

TEST_F(UnstableFixture, Restore)
{
    addEntry(mUnstable.get(), 1, 5);
    addSnapshot(mUnstable.get(), 1, 4);
    mUnstable->mOffset = 5;
    mUnstable->mOffsetInProgress = 6;
    mUnstable->mSnapshotInProgress = true;

    raft::Snapshot* snapshot2 = new raft::Snapshot;
    raft::SnapshotMetadata* meta2 = new raft::SnapshotMetadata;
    meta2->set_term(2);
    meta2->set_index(6);
    snapshot2->set_allocated_meta(meta2);

    mUnstable->restore(snapshot2);

    EXPECT_EQ(snapshot2->meta().index()+1, mUnstable->mOffset);
    EXPECT_EQ(snapshot2->meta().index()+1, mUnstable->mOffsetInProgress);
    EXPECT_EQ(mUnstable->mEntries.size(), 0);
    EXPECT_TRUE(snapshot2 == mUnstable->mSnapshot);
    EXPECT_FALSE(mUnstable->mSnapshotInProgress);
}

TEST_F(UnstableFixture, NextEntries)
{
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 5;

        std::vector<raft::LogEntry*> entries = unstable->nextEntries();
        EXPECT_EQ(entries.size(), 2);
        EXPECT_EQ(entries[0]->index, 5);
        EXPECT_EQ(entries[0]->term, 1);
        EXPECT_EQ(entries[1]->index, 6);
        EXPECT_EQ(entries[1]->term, 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;

        std::vector<raft::LogEntry*> entries = unstable->nextEntries();
        EXPECT_EQ(entries.size(), 1);
        EXPECT_EQ(entries[0]->index, 6);
        EXPECT_EQ(entries[0]->term, 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 7;

        std::vector<raft::LogEntry*> entries = unstable->nextEntries();
        EXPECT_EQ(entries.size(), 0);
    }
}


TEST_F(UnstableFixture, NextSnapshot)
{
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        EXPECT_EQ(unstable->nextSnapshot(), nullptr);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);
        EXPECT_EQ(unstable->nextSnapshot(), unstable->mSnapshot);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mSnapshotInProgress = true;
        EXPECT_EQ(unstable->nextSnapshot(), nullptr);
    }
}

TEST_F(UnstableFixture, AcceptInProgress)
{
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mSnapshotInProgress, false);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mSnapshotInProgress, false);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, false);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffsetInProgress = 6;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, false);
    }   

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffsetInProgress = 7;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, false);
    }

    // with snapshot
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 6;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 7;
        unstable->mSnapshotInProgress = false;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = true;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = true;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 5;
        unstable->mSnapshotInProgress = true;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 6;
        unstable->mSnapshotInProgress = true;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffsetInProgress = 7;
        unstable->mSnapshotInProgress = true;

        unstable->acceptInProgress();

        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mSnapshotInProgress, true);
    }
}

TEST_F(UnstableFixture, StableTo)
{
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        unstable->stableTo(5, 1);
        EXPECT_EQ(unstable->mOffset, 0);
        EXPECT_EQ(unstable->mOffsetInProgress, 0);
        EXPECT_EQ(unstable->mEntries.size(), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(5, 1);
        EXPECT_EQ(unstable->mOffset, 6);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(5, 1);
        EXPECT_EQ(unstable->mOffset, 6);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 7;
        unstable->stableTo(5, 1);
        EXPECT_EQ(unstable->mOffset, 6);
        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    // stable to the first entry and term mismatch
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 2, 6);
        unstable->mOffset = 6;
        unstable->mOffsetInProgress = 7;
        unstable->stableTo(6, 1);
        EXPECT_EQ(unstable->mOffset, 6);
        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    // stable to old entry
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(4, 1);
        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    // stable to old entry
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(4, 2);
        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    // stable to the first entry
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(5, 1);
        EXPECT_EQ(unstable->mOffset, 6);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 0);
    }

    // stable to the first entry
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(5, 1);
        EXPECT_EQ(unstable->mOffset, 6);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    // stable to the first entry
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 7;
        unstable->stableTo(5, 1);
        EXPECT_EQ(unstable->mOffset, 6);
        EXPECT_EQ(unstable->mOffsetInProgress, 7);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    // stable to the first entry
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(4, 1);
        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }

    // stable to the first entry
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 2, 5);
        addSnapshot(unstable.get(), 2, 4);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;
        unstable->stableTo(4, 1);
        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 1);
    }
}

TEST_F(UnstableFixture, TruncateAndAppend)
{
    // append to the end
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 5;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(6, 1));
        toAppend.push_back(makeEntry(7, 1));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mEntries.size(), 3);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 1);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 1);
        EXPECT_EQ(unstable->mEntries[2]->index, 7);
        EXPECT_EQ(unstable->mEntries[2]->term, 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(6, 1));
        toAppend.push_back(makeEntry(7, 1));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 3);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 1);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 1);
        EXPECT_EQ(unstable->mEntries[2]->index, 7);
        EXPECT_EQ(unstable->mEntries[2]->term, 1);
    }

    // replace the unstable entries
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 5;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(5, 2));
        toAppend.push_back(makeEntry(6, 2));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mEntries.size(), 2);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 2);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 2);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 5;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(4, 2));
        toAppend.push_back(makeEntry(5, 2));
        toAppend.push_back(makeEntry(6, 2));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 4);
        EXPECT_EQ(unstable->mOffsetInProgress, 4);
        EXPECT_EQ(unstable->mEntries.size(), 3);
        EXPECT_EQ(unstable->mEntries[0]->index, 4);
        EXPECT_EQ(unstable->mEntries[0]->term, 2);
        EXPECT_EQ(unstable->mEntries[1]->index, 5);
        EXPECT_EQ(unstable->mEntries[1]->term, 2);
        EXPECT_EQ(unstable->mEntries[2]->index, 6);
        EXPECT_EQ(unstable->mEntries[2]->term, 2);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(5, 2));
        toAppend.push_back(makeEntry(6, 2));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mEntries.size(), 2);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 2);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 2);
    }

    // truncate the existing entries and append
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addEntry(unstable.get(), 1, 7);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 5;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(6, 2));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mEntries.size(), 2);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 1);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 2);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addEntry(unstable.get(), 1, 7);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 5;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(7, 2));
        toAppend.push_back(makeEntry(8, 2));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 5);
        EXPECT_EQ(unstable->mEntries.size(), 4);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 1);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 1);
        EXPECT_EQ(unstable->mEntries[2]->index, 7);
        EXPECT_EQ(unstable->mEntries[2]->term, 2);
        EXPECT_EQ(unstable->mEntries[3]->index, 8);
        EXPECT_EQ(unstable->mEntries[3]->term, 2);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addEntry(unstable.get(), 1, 7);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(6, 2));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 2);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 1);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 2);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        addEntry(unstable.get(), 1, 7);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 7;

        std::vector<LogEntry*> toAppend;
        toAppend.push_back(makeEntry(6, 2));

        unstable->truncateAndAppend(toAppend);

        EXPECT_EQ(unstable->mOffset, 5);
        EXPECT_EQ(unstable->mOffsetInProgress, 6);
        EXPECT_EQ(unstable->mEntries.size(), 2);
        EXPECT_EQ(unstable->mEntries[0]->index, 5);
        EXPECT_EQ(unstable->mEntries[0]->term, 1);
        EXPECT_EQ(unstable->mEntries[1]->index, 6);
        EXPECT_EQ(unstable->mEntries[1]->term, 2);
    }
}