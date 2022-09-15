#include "src/raft_impl.h"
#include <butil/logging.h>

namespace raft
{

RaftImpl::RaftImpl(const Config& conf)
: mId(conf.id),
  mCurrentTerm(0),
  mLeaderId(0),
  mIsLearner(false),
  mElectionTimeout(conf.electionTick),
  mHeartbeatTimeout(conf.heartbeatTick)
{
}

void RaftImpl::Bootstrap()
{
    resetRandomizedElectionTimeout();
    becomeFollower(mCurrentTerm, NONE_LEADER_ID);
}

void RaftImpl::Tick()
{
    mTickfunc();
}

void RaftImpl::Campaign()
{
    becomeCandidate();
}

void RaftImpl::Propose()
{

}

void RaftImpl::ProposeConfChange()
{

}

void RaftImpl::Ready()
{

}

void RaftImpl::Step(RaftMessage& msg)
{
    if (msg.term == 0)
    {
        // local message
    }
    if (msg.term > mCurrentTerm)
    {
        ;
    }
    if (msg.term < mCurrentTerm)
    {
        ;
    }

    switch (msg.type)
    {
        case MsgHup:
        {
            hup();
        }

        default:
        {
            mStepfunc(msg);
        }
    }
}

void RaftImpl::Advance()
{

}

void RaftImpl::hup()
{
    if (mState == StateLeader)
    {
        LOG(INFO) << mId << " ignoring msg hup because already leader";
        return;
    }

    Campaign();
}

void RaftImpl::becomeFollower(uint64_t term, uint64_t leader)
{
    reset(term);
    mLeaderId = leader;
    mTickfunc = std::bind(&RaftImpl::tickElection, this);
    mStepfunc = std::bind(&RaftImpl::stepFollower, this, std::placeholders::_1);
    mState = StateFollower;
    LOG(INFO) << mId << " become follower at term " << term;
}

void RaftImpl::becomeCandidate()
{
    reset(mCurrentTerm + 1);
    mStepfunc = std::bind(&RaftImpl::stepCandidate, this, std::placeholders::_1);
    mTickfunc = std::bind(&RaftImpl::tickElection, this);
    mVote = mId;
    mState = StateCandidate;
    LOG(INFO) << mId << " become candidate at term " << mCurrentTerm;
}

void RaftImpl::reset(uint64_t term)
{
    mLeaderId = NONE_LEADER_ID;
    if (mCurrentTerm != term)
    {
        mCurrentTerm = term;
    }
    mElectionElapsed = 0;
    resetRandomizedElectionTimeout();
}

void RaftImpl::tickElection()
{
    LOG(INFO) << "tick election";
    mElectionElapsed++;
    if (pastElectionTimeout())
    {
        mElectionElapsed = 0;
        RaftMessage msg;
        msg.type = MsgHup;
        msg.from = mId;
        Step(msg);
    }
}

void RaftImpl::resetRandomizedElectionTimeout()
{
    mRandomizedElectionTimeout = mElectionTimeout + rand() % mElectionTimeout;
}

// pastElectionTimeout returns true iff r.electionElapsed is greater
// than or equal to the randomized election timeout in
// [electiontimeout, 2 * electiontimeout - 1].
bool RaftImpl::pastElectionTimeout()
{
	return mElectionElapsed >= mRandomizedElectionTimeout;
}

void RaftImpl::stepFollower(RaftMessage& msg)
{

}

void RaftImpl::stepCandidate(RaftMessage& msg)
{

}

}   