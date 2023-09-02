#ifndef __RAFT_LOG_H__
#define __RAFT_LOG_H__

#include "src/proto/raft.pb.h"
#include "raft.h"
#include "raft_unstable_log.h"

namespace raft
{

class RaftLog
{
public:
	RaftLog(Storage* stor, uint64_t maxApplyingEntsSize);

	std::string String();

private:
	// maybeAppend returns (0, false) if the entries cannot be appended. Otherwise,
	// it returns (last index of new entries, true).
	uint64_t maybeAppend(uint64_t index, uint64_t term, uint64_t committed, std::vector<LogEntry*> ents);
	
	uint64_t append(std::vector<LogEntry*> ents);

	// findConflict finds the index of the conflict.
	// It returns the first pair of conflicting entries between the existing
	// entries and the given entries, if there are any.
	// If there is no conflicting entries, and the existing entries contains
	// all the given entries, zero will be returned.
	// If there is no conflicting entries, but the given entries contains new
	// entries, the index of the first new entry will be returned.
	// An entry is considered to be conflicting if it has the same index but
	// a different term.
	// The index of the given entries MUST be continuously increasing.
	uint64_t findConflict(std::vector<LogEntry*> ents);

	// findConflictByTerm returns a best guess on where this log ends matching
	// another log, given that the only information known about the other log is the
	// (index, term) of its single entry.
	//
	// Specifically, the first returned value is the max guessIndex <= index, such
	// that term(guessIndex) <= term or term(guessIndex) is not known (because this
	// index is compacted or not yet stored).
	//
	// The second returned value is the term(guessIndex), or 0 if it is unknown.
	//
	// This function is used by a follower and leader to resolve log conflicts after
	// an unsuccessful append to a follower, and ultimately restore the steady flow
	// of appends.
	std::pair<uint64_t, uint64_t> findConflictByTerm(uint64_t index, uint64_t term);

	// nextUnstableEnts returns all entries that are available to be written to the
	// local stable log and are not already in-progress.
	void nextUnstableEnts();

	// hasNextUnstableEnts returns if there are any entries that are available to be
	// written to the local stable log and are not already in-progress.
	bool hasNextUnstableEnts();

	// hasNextOrInProgressUnstableEnts returns if there are any entries that are
	// available to be written to the local stable log or in the process of being
	// written to the local stable log.
	bool hasNextOrInProgressUnstableEnts();

	// nextCommittedEnts returns all the available entries for execution.
	// Entries can be committed even when the local raft instance has not durably
	// appended them to the local raft log yet. If allowUnstable is true, committed
	// entries from the unstable log may be returned; otherwise, only entries known
	// to reside locally on stable storage will be returned.
	void nextCommittedEnts();

	// hasNextCommittedEnts returns if there is any available entries for execution.
	// This is a fast check without heavy raftLog.slice() in nextCommittedEnts().
	bool hasNextCommittedEnts();

	// maxAppliableIndex returns the maximum committed index that can be applied.
	// If allowUnstable is true, committed entries from the unstable log can be
	// applied; otherwise, only entries known to reside locally on stable storage
	// can be applied.
	uint64_t maxAppliableIndex();

	// nextUnstableSnapshot returns the snapshot, if present, that is available to
	// be applied to the local storage and is not already in-progress.
	raft::Snapshot* nextUnstableSnapshot();

	// hasNextUnstableSnapshot returns if there is a snapshot that is available to
	// be applied to the local storage and is not already in-progress.
	bool hasNextUnstableSnapshot();

	// hasNextOrInProgressSnapshot returns if there is pending snapshot waiting for
	// applying or in the process of being applied.
	bool hasNextOrInProgressSnapshot();

	void snapshot();

	uint64_t firstIndex();

	uint64_t lastIndex();

	void commitTo(uint64_t tocommit);

	void appliedTo(uint64_t i, uint64_t size);

	void acceptApplying(uint64_t i, uint64_t size, bool allowUnstable);

	void stableTo(uint64_t i, uint64_t t);

	void stableSnapTo(uint64_t i);

	// acceptUnstable indicates that the application has started persisting the
	// unstable entries in storage, and that the current unstable entries are thus
	// to be marked as being in-progress, to avoid returning them with future calls
	// to Ready().
	void acceptUnstable();

	uint64_t lastTerm();

	uint64_t term(uint64_t i);

	std::vector<LogEntry*> entries(uint64_t i);

	std::vector<LogEntry*> allEntries();

	// isUpToDate determines if the given (lastIndex,term) log is more up-to-date
	// by comparing the index and term of the last entries in the existing logs.
	// If the logs have last entries with different terms, then the log with the
	// later term is more up-to-date. If the logs end with the same term, then
	// whichever log has the larger lastIndex is more up-to-date. If the logs are
	// the same, the given log is up-to-date.
	bool isUpToDate(uint64_t i, uint64_t term);

	bool matchTerm(uint64_t i, uint64_t term);

	bool maybeCommit(uint64_t maxIndex, uint64_t term);

	void restore(const raft::Snapshot& snapshot);

	// scan visits all log entries in the [lo, hi) range, returning them via the
	// given callback. The callback can be invoked multiple times, with consecutive
	// sub-ranges of the requested range. Returns up to pageSize bytes worth of
	// entries at a time. May return more if a single entry size exceeds the limit.
	//
	// The entries in [lo, hi) must exist, otherwise scan() eventually returns an
	// error (possibly after passing some entries through the callback).
	//
	// If the callback returns an error, scan terminates and returns this error
	// immediately. This can be used to stop the scan early ("break" the loop).
	void scan();

	// slice returns a slice of log entries from lo through hi-1, inclusive.
	Status slice(uint64_t low, uint64_t high, std::vector<LogEntry*>& ents);

	// l.firstIndex <= lo <= hi <= l.firstIndex + len(l.entries)
	Status mustCheckOutOfBounds(uint64_t low, uint64_t high);

	uint64_t zeroTermOnOutOfBounds(uint64_t term);

private:
    // storage contains all stable entries since the last snapshot.
    std::unique_ptr<Storage> mStorage;
    // unstable contains all unstable entries and snapshot.
    // they will be saved into storage.
    // unstable
	std::unique_ptr<RaftUnstable> mUnstable;

    // committed is the highest log position that is known to be in
	// stable storage on a quorum of nodes.
    uint64_t                 mCommitted;
    // applying is the highest log position that the application has
	// been instructed to apply to its state machine. Some of these
	// entries may be in the process of applying and have not yet
	// reached applied.
	// Use: The field is incremented when accepting a Ready struct.
	// Invariant: applied <= applying && applying <= committed
    uint64_t                 mApplying;
	// applied is the highest log position that the application has
	// successfully applied to its state machine.
	// Use: The field is incremented when advancing after the committed
	// entries in a Ready struct have been applied (either synchronously
	// or asynchronously).
	// Invariant: applied <= committed
    uint64_t                 mApplied;
	// maxApplyingEntsSize limits the outstanding byte size of the messages
	// returned from calls to nextCommittedEnts that have not been acknowledged
	// by a call to appliedTo.
	uint64_t    			mMaxApplyingEntsSize;
	// applyingEntsSize is the current outstanding byte size of the messages
	// returned from calls to nextCommittedEnts that have not been acknowledged
	// by a call to appliedTo.
	uint64_t 				mApplyingEntsSize;
	// applyingEntsPaused is true when entry application has been paused until
	// enough progress is acknowledged.
	bool					mApplyingEntsPaused;
};

}

#endif //__RAFT_LOG_H__