#ifndef __RAFT_PROGRESS_TRACKER_H__
#define __RAFT_PROGRESS_TRACKER_H__

#include <map>

namespace raft
{

class ProgressTracker
{
public:
    explicit ProgressTracker(int total);

    ~ProgressTracker();

    void RecordVote(uint64_t id, bool granted);

    void ResetVotes() { mVotes.clear(); }

    VoteResult TallyVotes();

private:
    int                      mTotalNodes;
    std::map<uint64_t, bool> mVotes;
};

}

#endif