#ifndef __RAFT_PROGRESS_TRACKER_H__
#define __RAFT_PROGRESS_TRACKER_H__

#include <map>
#include "src/progress.h"

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

    Progress* GetProgress(uint64_t id);

private:
    int                             mTotalNodes;
    std::map<uint64_t, bool>        mVotes;
    std::map<uint64_t, Progress*>   mProgress;
};

}

#endif
