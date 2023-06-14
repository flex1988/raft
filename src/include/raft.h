#ifndef __RAFT_H__
#define __RAFT_H__

#include <functional>
#include <vector>

#include "stdint.h"
#include "src/include/status.h"
#include "src/proto/raft.pb.h"

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
    std::vector<char*> entries;
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


class Storage
{
public:
    // InitialState returns the saved HardState and ConfState information.
    virtual void InitialState() = 0;

    // Entries returns a slice of consecutive log entries in the range [lo, hi),
    // starting from lo. The maxSize limits the total size of the log entries
	// returned, but Entries returns at least one entry if any.
	//
	// The caller of Entries owns the returned slice, and may append to it. The
	// individual entries in the slice must not be mutated, neither by the Storage
	// implementation nor the caller. Note that raft may forward these entries
	// back to the application via Ready struct, so the corresponding handler must
	// not mutate entries either (see comments in Ready struct).
	//
	// Since the caller may append to the returned slice, Storage implementation
	// must protect its state from corruption that such appends may cause. For
	// example, common ways to do so are:
	//  - allocate the slice before returning it (safest option),
	//  - return a slice protected by Go full slice expression, which causes
	//  copying on appends (see MemoryStorage).
	//
	// Returns ErrCompacted if entry lo has been compacted, or ErrUnavailable if
	// encountered an unavailable entry in [lo, hi).
    virtual Status Entries(uint64_t start, uint64_t end, std::vector<raft::LogEntry*>& entries) = 0;

    // Term returns the term of entry i, which must be in the range
	// [FirstIndex()-1, LastIndex()]. The term of the entry before
	// FirstIndex is retained for matching purposes even though the
	// rest of that entry may not be available.
    virtual uint64_t Term(uint64_t i) = 0;

    // LastIndex returns the index of the last entry in the log.
	virtual uint64_t LastIndex() = 0;

    // FirstIndex returns the index of the first log entry that is
	// possibly available via Entries (older entries have been incorporated
	// into the latest Snapshot; if storage only contains the dummy entry the
	// first log entry is not available).
	virtual uint64_t FirstIndex() = 0;

    // Snapshot returns the most recent snapshot.
	// If snapshot is temporarily unavailable, it should return ErrSnapshotTemporarilyUnavailable,
	// so raft state machine could know that Storage needs some time to prepare
	// snapshot and call Snapshot later.
    virtual void Snapshot() = 0;
};

}
#endif  // __RAFT_H__