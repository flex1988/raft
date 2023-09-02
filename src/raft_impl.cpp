#include <butil/logging.h>
#include <memory>

#include "raft_impl.h"
#include "progress_tracker.h"

namespace raft
{

RaftImpl::RaftImpl(const Config& conf)
: mId(conf.id),
  mCurrentTerm(0),
  mLeaderId(0),
  mIsLearner(false),
  mElectionTimeout(conf.electionTick),
  mHeartbeatTimeout(conf.heartbeatTick),
  mElectionElapsed(0),
  mHeartbeatElapsed(0)
{
    mTracker.reset(new ProgressTracker(static_cast<int>(conf.clusterIds.size())));
    for (uint32_t i = 0; i < conf.clusterIds.size(); i++)
    {
        mClusterIds.push_back(conf.clusterIds[i]);
    }
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
    poll(mId, MsgVote, true);
    becomeCandidate();

    for (uint32_t i = 0; i < mClusterIds.size(); i++)
    {
        if (mClusterIds[i] == mId)
        {
            continue;
        }
        RaftMessage msg;
        msg.term = mCurrentTerm;
        msg.to = mClusterIds[i];
        msg.type = MsgVote;
        //msg.index = 0;
        msg.logTerm = 0;
        submitMessage(msg);
    }
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

Status RaftImpl::Step(RaftMessage& msg)
{
    Status s = OK;
    if (msg.term == 0)
    {
        // local message
    }
    else if (msg.term > mCurrentTerm)
    {
        LOG(INFO) << mId << " [term: " << mCurrentTerm << "]" << "received a " << msg.type <<
                " message with higher term from " << msg.from << " [term: " << msg.term << "]";
        if (msg.type == MsgApp || msg.type == MsgHeartbeat || msg.type == MsgSnap)
        {
            becomeFollower(msg.term, msg.from);
        }
        else
        {
            becomeFollower(msg.term, NONE_LEADER_ID);
        }
    }
    else if (msg.term < mCurrentTerm)
    {
        LOG(INFO) << mId << " [term: " << mCurrentTerm << "] ignored a " << msg.type <<
                "message with lower term from " << msg.from << "[term: " << msg.term << "]";
        return s;
    }

    switch (msg.type)
    {
        case MsgHup:
        {
            hup();
        }

        default:
        {
            s = mStepfunc(msg);
        }
    }
    return s;
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

void RaftImpl::becomeLeader()
{
    CHECK_NE(mState, StateFollower) << "invalid transition [follower -> leader]";
    reset(mCurrentTerm);
    mLeaderId = mId;
    mTickfunc = std::bind(&RaftImpl::tickHeartbeat, this);
    mStepfunc = std::bind(&RaftImpl::stepLeader, this, std::placeholders::_1);
    mState = StateLeader;
    LOG(INFO) << mId << " become leader at term " << mCurrentTerm;
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
    mTickfunc = std::bind(&RaftImpl::tickElection, this);
    mStepfunc = std::bind(&RaftImpl::stepCandidate, this, std::placeholders::_1);
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

void RaftImpl::tickHeartbeat()
{
    mHeartbeatElapsed++;
    mElectionElapsed++;

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

VoteResult RaftImpl::poll(uint64_t id, RaftMessageType type, bool win)
{
    mTracker->RecordVote(id, win);
    return mTracker->TallyVotes();
}

Status RaftImpl::stepFollower(RaftMessage& msg)
{
    switch (msg.type)
    {
        case MsgProp:
        {
            break;
        }

        case MsgApp:
        {
            break;
        }

    }
    return OK;
}

Status RaftImpl::stepCandidate(RaftMessage& msg)
{
    switch (msg.type)
    {
        case MsgProp:
        {
            LOG(INFO) << mId << " no leader at term " << mCurrentTerm << " dropping proposal";
            return ERROR_PROPOSAL_DROPPED;
        }
        case MsgApp:
        {
            becomeFollower(msg.term, msg.from);
            // handleAppendEntries(msg);
        }
        case MsgHeartbeat:
        {
            becomeFollower(msg.term, msg.from);
            // handleHeartbeat(msg);
        }
        case MsgSnap:
        {
            becomeFollower(msg.term, msg.from);
            // handleSnapshot(msg);
        }
        case MsgVoteResp:
        {
            VoteResult res = poll(msg.from, msg.type, !msg.reject);
            LOG(INFO) << mId << " has received " << res.granted << " " << msg.type << " and " << res.rejected << " rejections";
            if (res.state == VoteWon)
            {
                becomeLeader();
                // bcastAppend();
            }
            else if (res.state == VoteLost)
            {
                becomeFollower(mCurrentTerm, NONE_LEADER_ID);
            }
        }
        case MsgTimeoutNow:
            LOG(INFO) << mId <<  "[term " << msg.term << " state " << mState << " ] ignored MsgTimeoutNow from " << msg.from;
    }
    return OK;
}

Status RaftImpl::stepLeader(RaftMessage& msg)
{
    switch (msg.type)
    {
        case MsgProp:
            if (msg.entries.empty())
            {
                assert(0);
            }
            if (GetProgress(mId) == NULL)
            {
                return ERROR_PROPOSAL_DROPPED;
            }
    }
    return OK;
}

bool RaftImpl::appendEntry(std::vector<raft::LogEntry*> entries)
{
    return true;
}

void RaftImpl::submitMessage(RaftMessage& msg)
{
    if (msg.from == NONE_LEADER_ID)
    {
        msg.from = mId;
    }
    if (msg.type == MsgVote || msg.type == MsgVoteResp)
    {
        // All {pre-,}campaign messages need to have the term set when
        // sending.
        // - MsgVote: m.Term is the term the node is campaigning for,
        //   non-zero as we increment the term when campaigning.
        // - MsgVoteResp: m.Term is the new r.Term if the MsgVote was
        //   granted, non-zero for the same reason MsgVote is
        // - MsgPreVote: m.Term is the term the node will campaign,
        //   non-zero as we use m.Term to indicate the next term we'll be
        //   campaigning for
        // - MsgPreVoteResp: m.Term is the term received in the original
        //   MsgPreVote if the pre-vote was granted, non-zero for the
        //   same reasons MsgPreVote is
        CHECK_NE(msg.term, 0) << "term should be set when sending " << msg.type;
    }
    else
    {
        CHECK_EQ(msg.term, 0) << "term should not be set when sending " << msg.type << " term " << msg.term;

        if (msg.type != MsgProp)
        {
            msg.term = mCurrentTerm;
        }
    }
    LOG(INFO) << mId << " submit msg to " << msg.to;
    mSendMsgs.push_back(msg);
}

}
