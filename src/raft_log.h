#ifdef __RAFT_LOG_H__
#define __RAFT_LOG_H__

namespace raft
{

class RaftLog
{
public:
    RaftLog();

private:
    // storage contains all stable entries since the last snapshot.
    std::unique_ptr<Storage> mStorage;
    // unstable contains all unstable entries and snapshot.
    // they will be saved into storage.
    // unstable

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
};

}

#endif //__RAFT_LOG_H__