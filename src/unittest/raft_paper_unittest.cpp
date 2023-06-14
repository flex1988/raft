#include "gtest/gtest.h"
#include "src/include/raft.h"
#include "src/unittest/raft_unittest_util.h"

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
            {
                raft->becomeFollower(1, 2);
                break;
            }
            case StateCandidate:
            {
                raft->becomeCandidate();
                break;
            }
            case StateLeader:
            {
                raft->becomeCandidate();
                raft->becomeLeader();
                break;
            }
        }

        RaftMessage msg;
        msg.type = MsgApp;
        msg.term = 2;
        raft->Step(msg);

        EXPECT_EQ(raft->mCurrentTerm, 2);
        EXPECT_EQ(raft->mState, StateFollower);
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

static bool reject_stale_term_called = false;
Status stepFake(RaftMessage& msg)
{
    reject_stale_term_called = true;
    return RAFT_OK;
}

// TestRejectStaleTermMessage tests that if a server receives a request with
// a stale term number, it rejects the request.
// Our implementation ignores the request instead.
// Reference: section 5.1
TEST_F(RaftPaperFixture, RejectStaleTermMessage)
{
    RaftImpl* raft = newRaft(1, 10, 1);
    raft->LoadState(2);
    raft->mStepfunc = std::bind(&stepFake, std::placeholders::_1);

    RaftMessage msg;
    msg.type = MsgApp;
    msg.term = raft->mCurrentTerm - 1;
    raft->Step(msg);

    EXPECT_FALSE(reject_stale_term_called);
}

// TestStartAsFollower tests that when servers start up, they begin as followers.
// Reference: section 5.2
TEST_F(RaftPaperFixture, StartAsFollower)
{
    RaftImpl* raft = newRaft(1, 10, 1);
    EXPECT_EQ(raft->mState, StateFollower);
}

// TestLeaderBcastBeat tests that if the leader receives a heartbeat tick,
// it will send a MsgHeartbeat with m.Index = 0, m.LogTerm=0 and empty entries
// as heartbeat to all followers.
// Reference: section 5.2
TEST_F(RaftPaperFixture, LeaderBcastBeat)
{
    RaftImpl* raft = newRaft(1, 10, 1);
    raft->becomeCandidate();
    raft->becomeLeader();
}

}
