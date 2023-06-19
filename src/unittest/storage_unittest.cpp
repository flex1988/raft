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
        log->set_index(index);
        log->set_term(term);
        if (data)
        {
            log->set_data(data);
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
            entries.push_back(allocEntry(logs[i], logs[i+1], NULL));
        }

        std::unique_ptr<MemoryStorage> mem = std::move(allocMemoryStorage(entries));    
        return mem;
    }
};

TEST_F(StorageFixture, Term)
{
    std::vector<raft::LogEntry*> entries;
    entries.push_back(allocEntry(3, 3, NULL));
    entries.push_back(allocEntry(4, 4, NULL));
    entries.push_back(allocEntry(5, 5, NULL));

    std::unique_ptr<MemoryStorage> mem = std::move(allocMemoryStorage(entries));

    EXPECT_EQ(mem->Term(2), 0);
    EXPECT_EQ(mem->Term(3), 3);
    EXPECT_EQ(mem->Term(4), 4);
    EXPECT_EQ(mem->Term(5), 5);
    EXPECT_EQ(mem->Term(6), 0);
}

TEST_F(StorageFixture, LastIndex)
{
    std::vector<raft::LogEntry*> entries;
    entries.push_back(allocEntry(3, 3, NULL));
    entries.push_back(allocEntry(4, 4, NULL));
    entries.push_back(allocEntry(5, 5, NULL));

    std::unique_ptr<MemoryStorage> mem = std::move(allocMemoryStorage(entries));
    
    EXPECT_EQ(mem->LastIndex(), 5);

    std::vector<raft::LogEntry*> newLog;
    newLog.push_back(allocEntry(6, 5, NULL));
    mem->Append(newLog);

    EXPECT_EQ(mem->LastIndex(), 6);
}

TEST_F(StorageFixture, FirstIndex)
{
    std::vector<raft::LogEntry*> entries;
    entries.push_back(allocEntry(3, 3, NULL));
    entries.push_back(allocEntry(4, 4, NULL));
    entries.push_back(allocEntry(5, 5, NULL));

    std::unique_ptr<MemoryStorage> mem = std::move(allocMemoryStorage(entries));

    EXPECT_EQ(mem->FirstIndex(), 4);
    // TODO Compact
}

TEST_F(StorageFixture, Entries)
{
    {
        int logs[8] = {3,3,4,4,5,5,6,6};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 8));
        std::vector<raft::LogEntry*> fetch;
        Status s = mem->Entries(2, 6, fetch);
        EXPECT_EQ(s.Code(), ERROR_MEMSTOR_COMPACTED.Code());
        EXPECT_TRUE(fetch.empty());
    }

    {
        int logs[8] = {3,3,4,4,5,5,6,6};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 8));
        std::vector<raft::LogEntry*> fetch;
        Status s = mem->Entries(3, 4, fetch);
        EXPECT_EQ(s.Code(), ERROR_MEMSTOR_COMPACTED.Code());
        EXPECT_TRUE(fetch.empty());
    }

    {
        int logs[8] = {3,3,4,4,5,5,6,6};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 8));
        std::vector<raft::LogEntry*> fetch;
        Status s = mem->Entries(4, 5, fetch);
        EXPECT_EQ(s.Code(), 0);
        EXPECT_EQ(fetch.size(), 1);
        EXPECT_EQ(fetch[0]->index(), 4);
        EXPECT_EQ(fetch[0]->term(), 4);
    }

    {
        int logs[8] = {3,3,4,4,5,5,6,6};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 8));
        std::vector<raft::LogEntry*> fetch;
        Status s = mem->Entries(4, 6, fetch);
        EXPECT_EQ(s.Code(), 0);
        EXPECT_EQ(fetch.size(), 2);
        EXPECT_EQ(fetch[0]->index(), 4);
        EXPECT_EQ(fetch[0]->term(), 4);
        EXPECT_EQ(fetch[1]->index(), 5);
        EXPECT_EQ(fetch[1]->term(), 5);
    }

    {
        int logs[8] = {3,3,4,4,5,5,6,6};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 8));
        std::vector<raft::LogEntry*> fetch;
        Status s = mem->Entries(4, 7, fetch);
        EXPECT_EQ(s.Code(), 0);
        EXPECT_EQ(fetch.size(), 3);
        EXPECT_EQ(fetch[0]->index(), 4);
        EXPECT_EQ(fetch[0]->term(), 4);
        EXPECT_EQ(fetch[1]->index(), 5);
        EXPECT_EQ(fetch[1]->term(), 5);
        EXPECT_EQ(fetch[2]->index(), 6);
        EXPECT_EQ(fetch[2]->term(), 6);
    }
}

TEST_F(StorageFixture, Append)
{
    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));
        std::vector<raft::LogEntry*> append;
        append.push_back(allocEntry(1, 1, NULL));
        append.push_back(allocEntry(2, 2, NULL));
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), 3);
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries[1]->index(), 4);
        EXPECT_EQ(mem->mEntries[1]->term(), 4);
        EXPECT_EQ(mem->mEntries[2]->index(), 5);
        EXPECT_EQ(mem->mEntries[2]->term(), 5);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));
        std::vector<raft::LogEntry*> append;
        append.push_back(allocEntry(3, 3, NULL));
        append.push_back(allocEntry(4, 4, NULL));
        append.push_back(allocEntry(5, 5, NULL));
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), 3);
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries[1]->index(), 4);
        EXPECT_EQ(mem->mEntries[1]->term(), 4);
        EXPECT_EQ(mem->mEntries[2]->index(), 5);
        EXPECT_EQ(mem->mEntries[2]->term(), 5);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));
        std::vector<raft::LogEntry*> append;
        append.push_back(allocEntry(3, 3, NULL));
        append.push_back(allocEntry(4, 6, NULL));
        append.push_back(allocEntry(5, 6, NULL));
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), 3);
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries[1]->index(), 4);
        EXPECT_EQ(mem->mEntries[1]->term(), 6);
        EXPECT_EQ(mem->mEntries[2]->index(), 5);
        EXPECT_EQ(mem->mEntries[2]->term(), 6);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));
        std::vector<raft::LogEntry*> append;
        append.push_back(allocEntry(3, 3, NULL));
        append.push_back(allocEntry(4, 4, NULL));
        append.push_back(allocEntry(5, 5, NULL));
        append.push_back(allocEntry(6, 5, NULL));
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), 4);
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries[1]->index(), 4);
        EXPECT_EQ(mem->mEntries[1]->term(), 4);
        EXPECT_EQ(mem->mEntries[2]->index(), 5);
        EXPECT_EQ(mem->mEntries[2]->term(), 5);
        EXPECT_EQ(mem->mEntries[3]->index(), 6);
        EXPECT_EQ(mem->mEntries[3]->term(), 5);
    }

    // Truncate incoming entries, truncate the existing entries and append.
    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));
        std::vector<raft::LogEntry*> append;
        append.push_back(allocEntry(2, 3, NULL));
        append.push_back(allocEntry(3, 3, NULL));
        append.push_back(allocEntry(4, 5, NULL));
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), 2);
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries[1]->index(), 4);
        EXPECT_EQ(mem->mEntries[1]->term(), 5);
    }

    // Truncate the existing entries and append.
    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));
        std::vector<raft::LogEntry*> append;
        append.push_back(allocEntry(4, 5, NULL));
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), 2);
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries[1]->index(), 4);
        EXPECT_EQ(mem->mEntries[1]->term(), 5);
    }

    // Direct append.
    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));
        std::vector<raft::LogEntry*> append;
        append.push_back(allocEntry(6, 5, NULL));
        mem->Append(append);

        EXPECT_EQ(mem->mEntries.size(), 4);
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries[1]->index(), 4);
        EXPECT_EQ(mem->mEntries[1]->term(), 4);
        EXPECT_EQ(mem->mEntries[2]->index(), 5);
        EXPECT_EQ(mem->mEntries[2]->term(), 5);
        EXPECT_EQ(mem->mEntries[3]->index(), 6);
        EXPECT_EQ(mem->mEntries[3]->term(), 5);
    }
}

TEST_F(StorageFixture, CreateSnapshot)
{
    int logs[6] = {3, 3, 4, 4, 5, 5};
    std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  

    {
        raft::ConfState* cs = new raft::ConfState;
        cs->add_voters(1);
        cs->add_voters(2);
        cs->add_voters(3);
        std::string* data = new std::string("data");
        raft::Snapshot snapshot;

        Status s = mem->CreateSnapshot(4, cs, data, &snapshot);
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

        Status s = mem->CreateSnapshot(5, cs, data, &snapshot);
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
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries.size(), 3);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  
        Status s = mem->Compact(3);
        EXPECT_EQ(s.Code(), ERROR_MEMSTOR_COMPACTED.Code());
        EXPECT_EQ(mem->mEntries[0]->index(), 3);
        EXPECT_EQ(mem->mEntries[0]->term(), 3);
        EXPECT_EQ(mem->mEntries.size(), 3);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  
        Status s = mem->Compact(4);
        EXPECT_TRUE(s.IsOK());
        EXPECT_EQ(mem->mEntries[0]->index(), 4);
        EXPECT_EQ(mem->mEntries[0]->term(), 4);
        EXPECT_EQ(mem->mEntries.size(), 2);
    }

    {
        int logs[6] = {3, 3, 4, 4, 5, 5};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 6));  
        Status s = mem->Compact(5);
        EXPECT_TRUE(s.IsOK());
        EXPECT_EQ(mem->mEntries[0]->index(), 5);
        EXPECT_EQ(mem->mEntries[0]->term(), 5);
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