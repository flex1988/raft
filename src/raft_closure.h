#ifndef __RAFT_CLOSURE__
#define __RAFT_CLOSURE__

#include <bthread/bthread.h>
#include "google/protobuf/stubs/callback.h"
#include "src/include/status.h"

namespace raft
{

template <typename T>
class RaftStatusClosure : public google::protobuf::Closure
{
public:
    typedef void (T::*M)(const Status&);

    RaftStatusClosure(T* t, M m) 
    : _t(t), _m(m)
    {}

    ~RaftStatusClosure() {}

    virtual void Run()
    {
        (_t->*_m)(_s);
        delete this;
    }

    void SetStatus(const Status& s)
    {
        _s = s;
    }

private:
    T*          _t;
    M           _m;
    Status      _s;
};

void run_closure_in_bthread(google::protobuf::Closure* closure,
                            bthread_attr_t attr);

}

#endif // __RAFT_CLOSURE__