#include "src/progress.h"

namespace raft
{

Progress::Progress()
: mMatchIndex(0),
  mNextIndex(0),
  mState(StateInitial),
  mRecentActive(false),
  mProbeSent(false)
{
}

void Progress::ResetState(ProgressState state)
{
    mProbeSent = false;
    mState = state;
    // mInflights.clear();
}

void Progress::BecomeReplicate()
{
    ResetState(StateReplicate);
    mNextIndex = mMatchIndex + 1;
}

}