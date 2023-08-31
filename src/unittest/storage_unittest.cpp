#include "gtest/gtest.h"
#include "src/include/status.h"
#include "src/unittest/raft_unittest_util.h"
#include "src/memory_storage.h"
#include <memory>

using namespace raft;

class StorageFixture : public ::testing::Test
{
public:
    raft::LogEntry* allocEntry(uint64_t index, uint64_t term, char* data)
    {
        raft::LogEntry* log = new raft::LogEntry;
        log->index = index;
        log->term = term;
        if (data)
        {
            log->data = data;
        }
        return log;
    }

    void freeEntry(raft::LogEntry* log)
    {
        delete log;
    }

    std::unique_ptr<MemoryStorage> allocMemoryStorage(std::vector<raft::LogEntry*>& entries)
    {
        std::unique_ptr<MemoryStorage> memStor = std::make_unique<MemoryStorage>();
        memStor->mEntries = entries;
        return memStor;
    }

    std::unique_ptr<MemoryStorage> prepareEntries(int logs[], int length)
    {
        std::vector<raft::LogEntry*> entries;
        for (int i = 0; i < length; i+=2)
        {
            entries.push_back(new LogEntry(logs[i], logs[i+1], NULL));
        }

        std::unique_ptr<MemoryStorage> mem = std::move(allocMemoryStorage(entries));    
        return mem;
    }

    void SetUp()
    {
        std::vector<raft::LogEntry*> entries;
        entries.push_back(allocEntry(3, 3, NULL));
        entries.push_back(allocEntry(4, 4, NULL));
        entries.push_back(allocEntry(5, 5, NULL));

        mMemStor = std::move(allocMemoryStorage(entries));
    }

    void TearDown()
    {
    }

    struct EntriesTestCase
    {
        std::vector<uint64_t> entries;
        uint64_t start;
        uint64_t end;
        int count;
        int errorcode;
        std::vector<uint64_t> result;
    };

    void runEntries(EntriesTestCase* t)
    {
        std::vector<raft::LogEntry*> entries;
        for (uint32_t i = 0; i < t->entries.size();)
        {
            entries.push_back(allocEntry(t->entries[i], t->entries[i+1], NULL));
            i += 2;
        }
        std::unique_ptr<MemoryStorage> mem = allocMemoryStorage(entries);
        std::vector<raft::LogEntry*> fetch;
        Status s = mem->Entries(t->start, t->end, fetch);
        EXPECT_EQ(s.Code(), t->errorcode);
        EXPECT_EQ(fetch.size(), t->count);
        for (int i = 0; i < t->count; i++)
        {
            EXPECT_EQ(fetch[i]->index, t->result[i*2]);
            EXPECT_EQ(fetch[i]->term, t->result[i*2 + 1]);
        }
    }

    struct AppendTestCase
    {
        std::vector<uint64_t> entries;
        std::vector<uint64_t> append;
        int count;
        std::vector<uint64_t> result;
    };

    void runAppend(AppendTestCase* t)
    {
        std::vector<raft::LogEntry*> entries;
        for (uint32_t i = 0; i < t->entries.size();)
        {
            entries.push_back(allocEntry(t->entries[i], t->entries[i+1], NULL));
            i += 2;
        }

        std::unique_ptr<MemoryStorage> mem = allocMemoryStorage(entries);

        std::vector<raft::LogEntry*> append;
        for (uint32_t i = 0; i < t->append.size();)
        {
            append.push_back(allocEntry(t->append[i], t->append[i+1], NULL));
            i += 2;
        }
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), t->count);
        for (uint32_t i = 0; i < mem->mEntries.size(); i++)
        {
            EXPECT_EQ(mem->mEntries[i]->index, t->result[2*i]);
            EXPECT_EQ(mem->mEntries[i]->term, t->result[2*i+1]);
        }
    }

    std::unique_ptr<MemoryStorage> mMemStor;
};

TEST_F(StorageFixture, Term)
{
    EXPECT_EQ(mMemStor->Term(2), 0);
    EXPECT_EQ(mMemStor->Term(3), 3);
    EXPECT_EQ(mMemStor->Term(4), 4);
    EXPECT_EQ(mMemStor->Term(5), 5);
    EXPECT_EQ(mMemStor->Term(6), 0);
}

TEST_F(StorageFixture, LastIndex)
{
    EXPECT_EQ(mMemStor->LastIndex(), 5);
    std::vector<raft::LogEntry*> newLog;
    newLog.push_back(allocEntry(6, 5, NULL));
    mMemStor->Append(newLog);
    EXPECT_EQ(mMemStor->LastIndex(), 6);
}

TEST_F(StorageFixture, FirstIndex)
{
    EXPECT_EQ(mMemStor->FirstIndex(), 4);
    // TODO Compact
}

TEST_F(StorageFixture, Entries)
{
    std::vector<EntriesTestCase> cases = {
        { {3,3,4,4,5,5,6,6}, 2, 6, 0, ERROR_MEMSTOR_COMPACTED.Code(), {} },
        { {3,3,4,4,5,5,6,6}, 3, 4, 0, ERROR_MEMSTOR_COMPACTED.Code(), {} },
        { {3,3,4,4,5,5,6,6}, 4, 5, 1, 0, {4, 4} },
        { {3,3,4,4,5,5,6,6}, 4, 6, 2, 0, {4, 4, 5, 5} },
        { {3,3,4,4,5,5,6,6}, 4, 7, 3, 0, {4, 4, 5, 5, 6, 6} }
    };

    for (uint32_t i = 0; i < cases.size(); i++)
    {
        runEntries(&cases[i]);
    }
}

TEST_F(StorageFixture, Append)
{
    std::vector<AppendTestCase> cases = {
        { {3, 3, 4, 4, 5, 5}, {1, 1, 2, 2}, 3, {3, 3, 4, 4, 5, 5} },
        { {3, 3, 4, 4, 5, 5}, {3, 3, 4, 4, 5, 5}, 3, {3, 3, 4, 4, 5, 5} },
        { {3, 3, 4, 4, 5, 5}, {3, 3, 4, 6, 5, 6}, 3, {3, 3, 4, 6, 5, 6} },
        { {3, 3, 4, 4, 5, 5}, {3, 3, 4, 4, 5, 5, 6, 5}, 4, {3, 3, 4, 4, 5, 5, 6, 5} },
        // Truncate incoming entries, truncate the existing entries and append.
        { {3, 3, 4, 4, 5, 5}, {2, 3, 3, 3, 4, 5}, 2, {3, 3, 4, 5} },
        // Truncate the existing entries and append.
        { {3, 3, 4, 4, 5, 5}, {4, 5}, 2, {3, 3, 4, 5} },
        // Direct append.
        { {3, 3, 4, 4, 5, 5}, {6, 5}, 4, {3, 3, 4, 4, 5, 5, 6, 5} },
    };
    for (uint32_t i = 0; i < cases.size(); i++)
    {
        runAppend(&cases[i]);
    }
}

TEST_F(StorageFixture, CreateSnapshot)
{
    {
        raft::ConfState* cs = new raft::ConfState;
        cs->add_voters(1);
        cs->add_voters(2);
        cs->add_voters(3);
        std::string* data = new std::string("data");
        raft::Snapshot snapshot;

        Status s = mMemStor->CreateSnapshot(4, cs, data, &snapshot);
        EXPECT_TRUE(s.IsOK());
        EXPECT_EQ(snapshot.meta().index(), 4);
        EXPECT_EQ(snapshot.meta().term(), 4);
        EXPECT_EQ(snapshot.meta().confstate().voters(0), 1);
        EXPECT_EQ(snapshot.meta().confstate().voters(1), 2);
        EXPECT_EQ(snapshot.meta().confstate().voters(2), 3);
    }

    {
        raft::ConfState* cs = new raft::ConfState;
        cs->add_voters(1);
        cs->add_voters(2);
        cs->add_voters(3);
        std::string* data = new std::string("data");
        raft::Snapshot snapshot;

        Status s = mMemStor->CreateSnapshot(5, cs, data, &snapshot);
        EXPECT_TRUE(s.IsOK());
        EXPECT_EQ(snapshot.meta().index(), 5);
        EXPECT_EQ(snapshot.meta().term(), 5);
        EXPECT_EQ(snapshot.meta().confstate().voters(0), 1);
        EXPECT_EQ(snapshot.meta().confstate().voters(1), 2);
        EXPECT_EQ(snapshot.meta().confstate().voters(2), 3);
    }
}

TEST_F(StorageFixture, Compact)
{
    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  
        Status s = mem->Compact(2);
        EXPECT_EQ(s.Code(), ERROR_MEMSTOR_COMPACTED.Code());
        EXPECT_EQ(mem->mEntries[0]->index, 3);
        EXPECT_EQ(mem->mEntries[0]->term, 3);
        EXPECT_EQ(mem->mEntries.size(), 3);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  
        Status s = mem->Compact(3);
        EXPECT_EQ(s.Code(), ERROR_MEMSTOR_COMPACTED.Code());
        EXPECT_EQ(mem->mEntries[0]->index, 3);
        EXPECT_EQ(mem->mEntries[0]->term, 3);
        EXPECT_EQ(mem->mEntries.size(), 3);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  
        Status s = mem->Compact(4);
        EXPECT_TRUE(s.IsOK());
        EXPECT_EQ(mem->mEntries[0]->index, 4);
        EXPECT_EQ(mem->mEntries[0]->term, 4);
        EXPECT_EQ(mem->mEntries.size(), 2);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  
        Status s = mem->Compact(5);
        EXPECT_TRUE(s.IsOK());
        EXPECT_EQ(mem->mEntries[0]->index, 5);
        EXPECT_EQ(mem->mEntries[0]->term, 5);
        EXPECT_EQ(mem->mEntries.size(), 1);
    }
}

TEST_F(StorageFixture, ApplySnapshot)
{
        raft::ConfState* cs1 = new raft::ConfState;
        cs1->add_voters(1);
        cs1->add_voters(2);
        cs1->add_voters(3);
        std::string* data1 = new std::string("data");
        std::unique_ptr<MemoryStorage> memStor = std::make_unique<MemoryStorage>();
        raft::Snapshot snapshot;
        snapshot.set_allocated_data(data1);
        snapshot.mutable_meta()->set_allocated_confstate(cs1);
        snapshot.mutable_meta()->set_index(4);
        snapshot.mutable_meta()->set_term(4);
        Status s = memStor->ApplySnapshot(snapshot);
        EXPECT_TRUE(s.IsOK());

        raft::ConfState* cs2 = new raft::ConfState;
        cs2->add_voters(1);
        cs2->add_voters(2);
        cs2->add_voters(3);
        raft::Snapshot snapshot2;
        std::string* data2 = new std::string("data");
        snapshot2.set_allocated_data(data2);
        snapshot2.mutable_meta()->set_allocated_confstate(cs2);
        snapshot2.mutable_meta()->set_index(3);
        snapshot2.mutable_meta()->set_term(3);
        Status s2 = memStor->ApplySnapshot(snapshot2);
        EXPECT_EQ(s2.Code(), ERROR_MEMSTOR_SNAP_OUTOFDATE.Code());
}