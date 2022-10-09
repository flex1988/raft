#include "gtest/gtest.h"
#include "src/include/raft.h"
#include "src/include/status.h"
#include "src/unittest/raft_unittest_util.h"

using namespace raft;

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
    Config conf;
    conf.electionTick = 10;
    conf.id = 0;
    conf.clusterIds.push_back(0);
    conf.clusterIds.push_back(1);
    conf.clusterIds.push_back(2);
    Raft* raft = new RaftImpl(conf);
    raft->Bootstrap();
    EXPECT_EQ(raft->GetState(), StateFollower);

    int electionTimeout = 25;
    for (int i = 0; i < electionTimeout; i++)
    {
        sleep(1);
        raft->Tick();
    }

    EXPECT_EQ(raft->GetState(), StateCandidate);
}

TEST_F(RaftFixture, LeaderElection)
{
    Config conf;
    conf.electionTick = 10;
    conf.id = 0;
    conf.clusterIds.push_back(0);
    conf.clusterIds.push_back(1);
    conf.clusterIds.push_back(2);
    Raft* raft = new RaftImpl(conf);
    raft->Bootstrap();
    EXPECT_EQ(raft->GetState(), StateFollower);
}

TEST_F(RaftFixture, ProgressLeader)
{
    RaftImpl* raft = newRaft(1, 5, 1);
    raft->becomeCandidate();
    raft->becomeLeader();
    raft->GetProgress(2)->BecomeReplicate();

    RaftMessage msg;
    msg.from = 1;
    msg.to = 1;
    msg.type = MsgProp;
    msg.entries.push_back("foo");
    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ(raft->Step(msg).Code(), 0);
    }
    EXPECT_EQ(raft->GetProgress(1)->mMatchIndex, 0);

}