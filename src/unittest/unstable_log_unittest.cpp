#include "gtest/gtest.h"
#include "status.h"
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
        log->set_index(index);
        log->set_term(term);
        unstable->mEntries.push_back(log);
    }

    raft::LogEntry* makeEntry(uint64_t index, uint64_t term)
    {
        raft::LogEntry* log = new raft::LogEntry;
        log->set_index(index);
        log->set_term(term);
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
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeFirstIndex(), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        EXPECT_EQ(unstable->maybeFirstIndex(), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeFirstIndex(), 5);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeFirstIndex(), 5);
    }
}

TEST_F(UnstableFixture, MaybeLastIndex)
{
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeLastIndex(), 5);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);

        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeLastIndex(), 5);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);

        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeLastIndex(), 4);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        EXPECT_EQ(unstable->maybeLastIndex(), 0);
    }
}




TEST_F(UnstableFixture, MaybeTerm)
{
    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(5), 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        mUnstable->mOffset = 5;

        EXPECT_EQ(mUnstable->maybeTerm(6), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(4), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(5), 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(6), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(4), 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(3), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(5), 0);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addSnapshot(unstable.get(), 1, 4);
        unstable->mOffset = 5;

        EXPECT_EQ(unstable->maybeTerm(4), 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);

        EXPECT_EQ(unstable->maybeTerm(5), 0);
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
        EXPECT_EQ(entries[0]->index(), 5);
        EXPECT_EQ(entries[0]->term(), 1);
        EXPECT_EQ(entries[1]->index(), 6);
        EXPECT_EQ(entries[1]->term(), 1);
    }

    {
        std::unique_ptr<RaftUnstable> unstable(new RaftUnstable);
        addEntry(unstable.get(), 1, 5);
        addEntry(unstable.get(), 1, 6);
        unstable->mOffset = 5;
        unstable->mOffsetInProgress = 6;

        std::vector<raft::LogEntry*> entries = unstable->nextEntries();
        EXPECT_EQ(entries.size(), 1);
        EXPECT_EQ(entries[0]->index(), 6);
        EXPECT_EQ(entries[0]->term(), 1);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 1);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 1);
        EXPECT_EQ(unstable->mEntries[2]->index(), 7);
        EXPECT_EQ(unstable->mEntries[2]->term(), 1);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 1);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 1);
        EXPECT_EQ(unstable->mEntries[2]->index(), 7);
        EXPECT_EQ(unstable->mEntries[2]->term(), 1);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 2);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 2);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 4);
        EXPECT_EQ(unstable->mEntries[0]->term(), 2);
        EXPECT_EQ(unstable->mEntries[1]->index(), 5);
        EXPECT_EQ(unstable->mEntries[1]->term(), 2);
        EXPECT_EQ(unstable->mEntries[2]->index(), 6);
        EXPECT_EQ(unstable->mEntries[2]->term(), 2);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 2);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 2);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 1);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 2);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 1);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 1);
        EXPECT_EQ(unstable->mEntries[2]->index(), 7);
        EXPECT_EQ(unstable->mEntries[2]->term(), 2);
        EXPECT_EQ(unstable->mEntries[3]->index(), 8);
        EXPECT_EQ(unstable->mEntries[3]->term(), 2);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 1);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 2);
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
        EXPECT_EQ(unstable->mEntries[0]->index(), 5);
        EXPECT_EQ(unstable->mEntries[0]->term(), 1);
        EXPECT_EQ(unstable->mEntries[1]->index(), 6);
        EXPECT_EQ(unstable->mEntries[1]->term(), 2);
    }
}