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

template <typename T, typename A1>
class RaftStatusClosureA1 : public google::protobuf::Closure
{
public:
    typedef void (T::*M)(A1, const Status&);

    RaftStatusClosureA1(T* t, A1 a1, M m) 
    : _t(t), _m(m), _a1(a1)
    {}

    ~RaftStatusClosureA1() {}

    virtual void Run()
    {
        (_t->*_m)(_a1, _s);
        delete this;
    }

    void SetStatus(const Status& s)
    {
        _s = s;
    }

private:
    T*          _t;
    M           _m;
    A1          _a1;
    Status      _s;
};

void run_closure_in_bthread(google::protobuf::Closure* closure,
                            bthread_attr_t attr);

template<typename T>
RaftStatusClosure<T>* NewStatusCallback(T* t, void (T::*m)(const Status&))
{
    return new RaftStatusClosure<T>(t, m);
}

template<typename T, typename A1>
RaftStatusClosureA1<T, A1>* NewStatusCallbackA1(T* t, A1 a1, void (T::*m)(A1, const Status&))
{
    return new RaftStatusClosureA1<T, A1>(t, a1, m);
}

}

#endif // __RAFT_CLOSURE__