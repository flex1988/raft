#ifndef __RAFT_UNITTEST_UTIL_H__
#define __RAFT_UNITTEST_UTIL_H__

#define private public
#include "src/raft_impl.h"

namespace raft
{

RaftImpl* newRaft(uint64_t id, int election, int heartbeat);

}

#endif  // __RAFT_UNITTEST_UTIL_H__