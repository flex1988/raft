#include "src/raft_closure.h"

namespace raft
{

static void* run_closure(void* arg)
{
    google::protobuf::Closure *c = static_cast<google::protobuf::Closure*>(arg);
    if (c)
    {
        c->Run();
    }
    return NULL;
}

void run_closure_in_bthread(google::protobuf::Closure* closure,
                            bthread_attr_t attr)
{
    bthread_t tid;
    int ret = bthread_start_background(&tid, &attr, run_closure, closure);
    if (0 != ret)
    {
        return closure->Run();
    }
}

}