#include "src/include/raft.h"
#include "src/progress_tracker.h"

namespace raft
{

ProgressTracker::ProgressTracker(int total)
:  mTotalNodes(total)
{
}

ProgressTracker::~ProgressTracker()
{
    mVotes.clear();
}

void ProgressTracker::RecordVote(uint64_t id, bool granted)
{
    mVotes[id] = granted;
}

VoteResult ProgressTracker::TallyVotes()
{
    VoteResult result;
    for (std::map<uint64_t, bool>::iterator iter = mVotes.begin(); iter != mVotes.end(); iter++)
    {
        if (iter->second)
        {
            result.granted++;
        }
        else
        {
            result.rejected++;
        }
    }
    if (result.granted > (mTotalNodes / 2))
    {
        result.state = VoteWon;
    }
    else if (result.granted + result.rejected < mTotalNodes)
    {
        result.state = VoteLost;
    }
    else
    {
        result.state = VoteNotDone;
    }
    return result;
}

Progress* ProgressTracker::GetProgress(uint64_t id)
{
    if (mProgress.find(id) == mProgress.end())
    {
        mProgress[id] = new Progress;
    }
    return mProgress[id];
}

}