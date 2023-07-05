#ifndef __RAFT_UNSTABLE_LOG_H__
#define __RAFT_UNSTABLE_LOG_H__

#include "src/proto/raft.pb.h"

namespace raft
{

class RaftUnstable
{
public:
    RaftUnstable();

    ~RaftUnstable();

private:
    // maybeFirstIndex returns the index of the first possible entry in entries
    // if it has a snapshot.
    uint64_t maybeFirstIndex();

    // maybeLastIndex returns the last index if it has at least one
    // unstable entry or snapshot.
    uint64_t maybeLastIndex();

    // maybeTerm returns the term of the entry at index i, if there
    // is any.
    uint64_t maybeTerm(uint64_t i);

    // nextEntries returns the unstable entries that are not already in the process
    // of being written to storage.
    std::vector<raft::LogEntry*> nextEntries();

    // nextSnapshot returns the unstable snapshot, if one exists that is not already
    // in the process of being written to storage.
    raft::Snapshot* nextSnapshot();

    // acceptInProgress marks all entries and the snapshot, if any, in the unstable
    // as having begun the process of being written to storage. The entries/snapshot
    // will no longer be returned from nextEntries/nextSnapshot. However, new
    // entries/snapshots added after a call to acceptInProgress will be returned
    // from those methods, until the next call to acceptInProgress.
    void acceptInProgress();

    // stableTo marks entries up to the entry with the specified (index, term) as
    // being successfully written to stable storage.
    //
    // The method should only be called when the caller can attest that the entries
    // can not be overwritten by an in-progress log append. See the related comment
    // in newStorageAppendRespMsg.
    void stableTo(uint64_t i, uint64_t t);

    // shrinkEntriesArray discards the underlying array used by the entries slice
    // if most of it isn't being used. This avoids holding references to a bunch of
    // potentially large entries that aren't needed anymore. Simply clearing the
    // entries wouldn't be safe because clients might still be using them.
    void shrinkEntriesArray();

    void stableSnapTo(uint64_t i);

    void restore(raft::Snapshot* snapshot);

    void truncateAndAppend(std::vector<raft::LogEntry*> entries);

    // slice returns the entries from the unstable log with indexes in the range
    // [lo, hi). The entire range must be stored in the unstable log or the method
    // will panic. The returned slice can be appended to, but the entries in it must
    // not be changed because they are still shared with unstable.
    //
    // TODO(pavelkalinnikov): this, and similar []pb.Entry slices, may bubble up all
    // the way to the application code through Ready struct. Protect other slices
    // similarly, and document how the client can use them.
    std::vector<raft::LogEntry*> slice(uint64_t low, uint64_t high);

    void mustCheckOutOfBounds(uint64_t low, uint64_t high);

private:
    // the incoming unstable snapshot, if any.
    raft::Snapshot*                 mSnapshot;
    // all entries that have not yet been written to storage.
    std::vector<raft::LogEntry*>    mEntries;
    // entries[i] has raft log position i+offset.
    uint64_t                        mOffset;
    // if true, snapshot is being written to storage.
    bool                            mSnapshotInProgress;
	// entries[:offsetInProgress-offset] are being written to storage.
	// Like offset, offsetInProgress is exclusive, meaning that it
	// contains the index following the largest in-progress entry.
	// Invariant: offset <= offsetInProgress
    uint64_t                        mOffsetInProgress;

};

}
#endif //__RAFT_UNSTABLE_LOG_H__