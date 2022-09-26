#define private public

#include "gtest/gtest.h"
#include "src/include/raft.h"
#include "src/raft_impl.h"


namespace raft
{

class RaftPaperFixture : public ::testing::Test
{
public:
    void SetUp()
    {
    }

    void TearDown()
    {
    }

    void testUpdateTermFromMessage(StateType state)
    {
        RaftImpl* raft = newRaft(1, 10, 1);

        switch (state)
        {
            case StateFollower:
                raft->becomeFollower(1, 2);
            case StateCandidate:
                raft->becomeCandidate();
            case StateLeader:
                raft->becomeCandidate();
                raft->becomeLeader();
        }

        RaftMessage msg;
        msg.type = MsgApp;
        msg.term = 2;
        raft->Step(msg);

        EXPECT_EQ(raft->mCurrentTerm, 2);
        EXPECT_EQ(raft->mState, StateFollower);
    }

    RaftImpl* newRaft(uint64_t id, int election, int heartbeat)
    {
        raft::Config conf;
        conf.electionTick = election;
        conf.heartbeatTick = heartbeat;
        conf.id = id;
        raft::RaftImpl* raft = new raft::RaftImpl(conf);
        raft->Bootstrap();
        return raft;
    }
};

TEST_F(RaftPaperFixture, FollowerUpdateTermFromMessage)
{
    testUpdateTermFromMessage(StateFollower);
}

TEST_F(RaftPaperFixture, CandidateUpdateTermFromMessage)
{
    testUpdateTermFromMessage(StateCandidate);
}

TEST_F(RaftPaperFixture, LeaderUpdateTermFromMessage)
{
    testUpdateTermFromMessage(StateLeader);
}

}