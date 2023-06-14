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
        EXPECT_EQ(s.Code(), ERROR_MEMORYSTORAGE_COMPACTED.Code());
        EXPECT_TRUE(fetch.empty());
    }

    {
        int logs[8] = {3,3,4,4,5,5,6,6};
        std::unique_ptr<MemoryStorage> mem = std::move(prepareEntries(logs, 8));
        std::vector<raft::LogEntry*> fetch;
        Status s = mem->Entries(3, 4, fetch);
        EXPECT_EQ(s.Code(), ERROR_MEMORYSTORAGE_COMPACTED.Code());
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
}