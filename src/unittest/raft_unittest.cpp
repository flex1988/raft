#include "gtest/gtest.h"
#include "src/include/raft.h"
#include "src/raft_impl.h"

class RaftFixture : public ::testing::Test
{
public:
    void SetUp()
    {
    }

    void TearDown()
    {
    }
};


TEST_F(RaftFixture, Tick)
{
    raft::Config conf;
    conf.electionTick = 10;
    raft::Raft* raft = new raft::RaftImpl(conf);
    raft->Bootstrap();
    EXPECT_EQ(raft->GetState(), raft::StateFollower);

    int electionTimeout = 25;
    for (int i = 0; i < electionTimeout; i++)
    {
        sleep(1);
        raft->Tick();
    }

    EXPECT_EQ(raft->GetState(), raft::StateCandidate);
}