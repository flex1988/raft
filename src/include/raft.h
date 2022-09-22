#ifndef __RAFT_H__
#define __RAFT_H__

#include <functional>
#include <vector>

#include "stdint.h"
#include "src/include/status.h"

namespace raft
{

const uint64_t NONE_LEADER_ID = 0;

enum RaftMessageType
{
    MsgNope = 0,
    MsgBeat,
    MsgHup,
    MsgProp,
    MsgApp,
    MsgVote,
    MsgVoteResp,
    MsgHeartbeat,
    MsgSnap,
    MsgTimeout,
    MsgTimeoutNow,
};

struct RaftMessage
{
    RaftMessageType type;
    uint64_t        to;
    uint64_t        from;
    uint64_t        term;
    // logTerm is generally used for appending Raft logs to followers. For example,
	// (type=MsgApp,index=100,logTerm=5) means leader appends entries starting at
	// index=101, and the term of entry at index 100 is 5.
	// (type=MsgAppResp,reject=true,index=100,logTerm=5) means follower rejects some
	// entries from its leader as it already has an entry with term 5 at index 100.
    uint64_t        logTerm;
    uint64_t        index;
    //              entry
    uint64_t        commit;
    bool            reject;
    RaftMessage()
    : type(MsgNope),
      to(0),
      from(0),
      term(0),
      logTerm(0),
      index(0),
      commit(0),
      reject(false)
    {}
};

enum VoteState
{
    VoteNotDone = 0,
    VoteWon = 1,
    VoteLost = 2,
};

struct VoteResult
{
    uint64_t    id;
    int         granted;
    int         rejected;
    VoteState   state;
    VoteResult()
    :  id(NONE_LEADER_ID),
       granted(0),
       rejected(0),
       state(VoteNotDone)
    {
    }
};

enum StateType
{
    StateFollower = 0,
    StateCandidate = 1,
    StateLeader = 2,
};

// Config contains the parameters to start a raft.
struct Config
{
    // ID is the identity of the local raft. ID cannot be 0.
    uint64_t                id;

    // ElectionTick is the number of Node.Tick invocations that must pass between
	// elections. That is, if a follower does not receive any message from the
	// leader of current term before ElectionTick has elapsed, it will become
	// candidate and start an election. ElectionTick must be greater than
	// HeartbeatTick. We suggest ElectionTick = 10 * HeartbeatTick to avoid
	// unnecessary leader switching.
    int                     electionTick;

    // HeartbeatTick is the number of Node.Tick invocations that must pass between
	// heartbeats. That is, a leader sends heartbeat messages to maintain its
	// leadership every HeartbeatTick ticks.
    int                     heartbeatTick;

	// Storage is the storage for raft. raft generates entries and states to be
	// stored in storage. raft reads the persisted entries and states out of
	// Storage when it needs. raft reads out the previous state and configuration
	// out of storage when restarting.

    // Storage  storage;

    // Applied is the last applied index. It should only be set when restarting
	// raft. raft will not return entries to the application smaller or equal to
	// Applied. If Applied is unset when restarting, raft might return previous
	// applied entries. This is a very application dependent configuration.
    uint64_t                applied;

    std::vector<uint64_t>   clusterIds;
    Config()
    : id(0),
      electionTick(0),
      heartbeatTick(0),
      applied(0)
    {
    }
};

class Raft
{
public:
    virtual void Bootstrap() = 0;

    virtual void Tick() = 0;

    virtual void Campaign() = 0;

    virtual void Propose() = 0;

    virtual void ProposeConfChange() = 0;

    virtual Status Step(RaftMessage&) = 0;

    virtual void Ready() = 0;

    virtual void Advance() = 0;

    virtual StateType GetState() const = 0;
};

}
#endif  // __RAFT_H__