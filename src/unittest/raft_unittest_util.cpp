#include "src/unittest/raft_unittest_util.h"

namespace raft
{
    RaftImpl* newRaft(uint64_t id, int election, int heartbeat)
    {
        raft::Config conf;
        conf.electionTick = election;
        conf.heartbeatTick = heartbeat;
        conf.id = id;
        raft::RaftImpl* raft = new raft::RaftImpl(conf);
        raft->Bootstrap();
        return raft;
    }
}