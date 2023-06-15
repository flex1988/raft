#include "src/include/status.h"

namespace raft 
{

Status RAFT_OK = Status(0, "");
Status RAFT_ERROR = Status(-1, "default error");
Status ERROR_PROPOSAL_DROPPED = Status(-1000, "error proposal dropped");
Status ERROR_MEMSTOR_COMPACTED = Status(-1001, "requested index is unavailable due to compaction");
Status ERROR_MEMSTOR_SNAP_OUTOFDATE = Status(-1002, "requested index is older than the existing snapshot");
Status ERROR_MEMSTOR_UNAVAILABLE = Status(-1003, "requested entry at index is unavailable");

}