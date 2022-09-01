#ifndef __CONSENSUS_NODE_IMPL__
#define __CONSENSUS_NODE_IMPL__

#include <bthread/bthread.h>
#include <bthread/unstable.h>
#include <brpc/channel.h>
#include <vector>
#include <map>

#include "src/include/consensus_node.h"
#include "src/proto/raft.pb.h"
#include "src/raft_closure.h"

namespace raft
{

class KVServiceImpl;

struct RaftTimer
{
    RaftTimer()
    : callback(NULL)
    {
    }

    // Returns: 0 - exist & not-run; 1 - still running; EINVAL - not exist.
    // bthread_timer_del
    void AddTimer(timespec time, google::protobuf::Closure* cb)
    {
        int ret = bthread_timer_del(timerid);
        if (callback != NULL && ret == 0)
        {
            delete callback;
            callback = NULL;
        }
        callback = cb;
        bthread_timer_add(&timerid,
            time,
            OnTimedout,
            cb);
    }

    void Reset()
    {
        int ret = bthread_timer_del(timerid);
        if (callback != NULL && ret == 0)
        {
            delete callback;
            callback = NULL;
        }
    }

    static void OnTimedout(void* arg)
    {
        reinterpret_cast<google::protobuf::Closure*>(arg)->Run();
    }

    bthread_timer_t            timerid;
    google::protobuf::Closure* callback;
};

class ConsensusNodeImpl
{
public:
    ConsensusNodeImpl()
    : mCurrentTerm(0),
      mLatestLogIndex(0),
      mCommitIndex(0),
      mLastApplied(0),
      mRole(RoleType::INITIAL),
      mRequestVoteSucc(0),
      mRequestVoteTerm(0),
      mIsBackgroundAEOngoing(false),
      mInflightAppendEntries(0)
    {
    }

    ~ConsensusNodeImpl() {}


    void AddPeer(raft::ServerId id);

    void SetOwnId(raft::ServerId id)
    {
        mOwnId = id;
    }

    std::string GetRoleStr();

    raft::LogEntry* GetLastLogEntry()
    {
        if (mLogs.empty())
        {
            return NULL;
        }
        return &mLogs.back();
    }

    void LeaderSendHeartBeats();

    bool IsLeader() { return mRole == raft::LEADER; }

    raft::ServerId Id() { return mOwnId; }

public:
    void HandleRequestVote(google::protobuf::RpcController* ctrl,
                                const RequestVoteRequest* request,
                                RequestVoteResponse* response,
                                google::protobuf::Closure* done);

    void HandleAppendEntries(google::protobuf::RpcController* ctrl,
                                const AppendEntriesRequest* request,
                                AppendEntriesResponse* response,
                                google::protobuf::Closure* done);

    void Submit(const raft::Command& cmd, raft::RaftStatusClosureA1<KVServiceImpl, google::protobuf::Closure*>* done);

    void Start(uint64_t term)
    {
        init();
        becomeFollower(term);
    }

private:
    struct AppendEntriesContext
    {
        int from;
        int index;
        int entryCount;
        AppendEntriesContext()
        : from(0),
          index(0),
          entryCount(0)
        {
        }
    };
    void init();
    void becomeLeader();
    void becomeFollower(uint64_t term);
    void becomeCandidate();

    void leaderReplicateLog();

    void applyLogToStateMachine();
    void onRequestVoteDone(brpc::Controller* ctrl, raft::RequestVoteResponse* response, uint64_t term);
    void onAppendEntriesDone(brpc::Controller* ctrl, raft::AppendEntriesResponse* response, AppendEntriesContext* context);
    void onHeartBeatDone(brpc::Controller* ctrl, raft::AppendEntriesResponse* response, uint64_t term);
    void onElectionTimeout(uint64_t term);
    void onHeartBeatTimeout();
    void onSubmitDone(const Status& status);

private:
    uint64_t                            mCurrentTerm;   
    raft::ServerId                      mVotedFor;
    std::vector<raft::LogEntry>         mLogs;   
    uint64_t                            mLatestLogIndex;
    // Volatile state on all servers:
    // index of highest log entry known to be committed (initialized to 0, increases monotonically)
    uint64_t                            mCommitIndex;
    // index of highest log entry applied to state machine (initialized to 0, increases monotonically)
    uint64_t                            mLastApplied;
    RoleType                            mRole;
    // Volatile state on leaders:
    // (Reinitialized after election)
    //
    // for each servers, index of the next log entry to send to that server
    // (initialized to leader last log index + 1)
    std::vector<uint64_t>               mNextIndex;
    // for each server, index of highest log entry known to be replicated on server
    // (initialized to 0, increases monotonically)

    // The difference here is because these two fields are used for different purposes:
    // matchIndex is an accurate value indicating the index up to which all the log entries in leader
    // and follower match. However, nextIndex is only an optimistic "guess" indicating which index
    // the leader should try for the next AppendEntries operation, it can be a good guess (i.e. it equals matchIndex + 1)
    // in which case the AppendEntries operation will succeed, but it can also be a bad guess
    // (e.g. in the case when a leader was just initiated) in which case the AppendEntries will fail
    // so that the leader will decrement nextIndex and retry.
    std::vector<uint64_t>               mMatchIndex;
    std::vector<raft::ServerId>         mPeers;
    brpc::Channel                       mChannels[5];
    raft::ServerId                      mOwnId;
    uint32_t                            mCurrentVote;
    uint32_t                            mRequestVoteSucc;
    uint64_t                            mRequestVoteTerm;
    RaftTimer                           mHeartBeatTimer;
    RaftTimer                           mElectionTimer;
    bool                                mIsBackgroundAEOngoing;
    int                                 mInflightAppendEntries;
    std::map<uint64_t, RaftStatusClosureA1<KVServiceImpl, google::protobuf::Closure*>* > mSubmitCallbacks;
};

}

#endif // __CONSENSUS_NODE_IMPL__