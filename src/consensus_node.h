#include <bthread/bthread.h>
#include <bthread/unstable.h>
#include <brpc/channel.h>
#include "src/proto/raft.pb.h"
#include <vector>

namespace raft
{

class ConsensusNode
{
public:
    ConsensusNode()
    : mCurrentTerm(0),
      mCommitIndex(0),
      mLastApplied(0),
      mRole(RoleType::INITIAL),
      mRequestVoteSucc(0),
      mRequestVoteTerm(0),
      mElectionTimerTerm(0)
    {
    }

    ~ConsensusNode() {}

    void Init();

    void AddServer(raft::ServerId id);

    void SetOwnId(raft::ServerId id)
    {
        mOwnId = id;
    }

    uint64_t GetCurrentTerm() { return mCurrentTerm; }

    uint64_t GetElectionTerm() { return mElectionTimerTerm; }

    void SetTerm(uint64_t term) { mCurrentTerm = term; }

    std::string GetRoleStr();

    raft::RoleType GetRole() { return mRole; }

    void SetVotedFor(const raft::ServerId& id)
    {
        mVotedFor = id;
    }

    bool HasVotedFor()
    {
        return !mVotedFor.ip().empty();
    }

    raft::LogEntry* GetLastLogEntry()
    {
        if (mLogs.empty())
        {
            return NULL;
        }
        return &mLogs.back();
    }

    void AddElectionTimer();

    void RemoveElectionTimer()
    {
        bthread_timer_del(mElectionTimer);
    }

    void AddHeartBeatTimer();

    void RemoveHeartBeatTimer()
    {
        bthread_timer_del(mHeartBeatTimer);
    }

    void RefreshElectionTimer();

    void BecomeFollower(uint64_t term);

    void StartElection();

    void StartLeader();

    void LeaderSendHeartBeats();

private:

    void onRequestVoteDone(brpc::Controller* ctrl, raft::RequestVoteResponse* response, uint64_t term);
    void onHeartBeatDone(brpc::Controller* ctrl, raft::AppendEntriesResponse* response, uint64_t term);

private:
    uint64_t                            mCurrentTerm;   
    raft::ServerId                      mVotedFor;
    std::vector<raft::LogEntry>         mLogs;   
    uint64_t                            mCommitIndex;
    uint64_t                            mLastApplied;
    RoleType                            mRole;
    std::vector<uint64_t>               mNextIndex;
    std::vector<uint64_t>               mMatchIndex;
    std::vector<raft::ServerId>         mServerIds;
    brpc::Channel                       mChannels[5];
    raft::ServerId                      mOwnId;
    bthread_timer_t                     mElectionTimer;
    bthread_timer_t                     mHeartBeatTimer;
    uint32_t                            mCurrentVote;
    uint32_t                            mRequestVoteSucc;
    uint64_t                            mRequestVoteTerm;
    uint64_t                            mElectionTimerTerm;
};

}