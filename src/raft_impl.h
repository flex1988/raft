#ifndef __RAFT_IMPL__
#define __RAFT_IMPL__

#include <memory>
#include <vector>

#include "src/include/raft.h"
#include "src/include/status.h"
#include "src/progress_tracker.h"

namespace raft
{

class RaftImpl : public Raft
{
public:
    RaftImpl(const Config& c);

    void Bootstrap();
    void Tick();
    void Campaign();
    void Propose();
    void ProposeConfChange();
    Status Step(RaftMessage&);
    void Ready();
    void Advance();

    StateType GetState() const { return mState; }
    Progress* GetProgress(uint64_t id) const { return mTracker->GetProgress(id);}

private:
    void becomeFollower(uint64_t term, uint64_t leader);
    void becomeLeader();
    void becomeCandidate();
    Status stepFollower(RaftMessage&);
    Status stepCandidate(RaftMessage&);
    Status stepLeader(RaftMessage&);
    void tickElection();
    void tickHeartbeat();
    void reset(uint64_t);
    void hup();
    VoteResult poll(uint64_t id, RaftMessageType type, bool win);
    void resetRandomizedElectionTimeout();
    bool pastElectionTimeout();
    void submitMessage(RaftMessage&);

private:
    uint64_t                                mId;
    std::vector<uint64_t>                   mPeerIds;
    uint64_t                                mCurrentTerm;
    uint64_t                                mVote;
    uint64_t                                mLeaderId;
    bool                                    mIsLearner;
    int                                     mHeartbeatTimeout;
    int                                     mElectionTimeout;

    // randomizedElectionTimeout is a random number between
	// [electiontimeout, 2 * electiontimeout - 1]. It gets reset
	// when raft changes its state to follower or candidate.
	int                                     mRandomizedElectionTimeout;

    // number of ticks since it reached last electionTimeout when it is leader
	// or candidate.
	// number of ticks since it reached last electionTimeout or received a
	// valid message from current leader when it is a follower.
    int                                     mElectionElapsed;
    int                                     mHeartbeatElapsed;
    // raft log
    uint64_t                                mMaxMsgSize;
    uint64_t                                mMaxUncommittedSize;
    StateType                               mState;
    std::function<void()>                   mTickfunc;
    std::function<Status(RaftMessage&)>     mStepfunc;
    std::vector<RaftMessage>                mSendMsgs;
    std::unique_ptr<ProgressTracker>        mTracker;
    std::vector<uint64_t>                   mClusterIds;
};

}

#endif  // __RAFT_IMPL__