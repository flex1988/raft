#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

#include <mutex>
#include <condition_variable>
#include <vector>

#include "src/include/kv_client.h"
#include "src/include/raft_types.h"
#include "src/client/kv_client_impl.h"

namespace raft
{

class Benchmark;

enum BenchmarkMode
{
    GET     = 0,
    PUT     = 1,
    DELETE  = 2,
    MIXED   = 3
};

struct OpStat
{
    uint64_t total_count;
    uint64_t total_cost;
    uint64_t total_put;
    uint64_t total_get;
    uint64_t total_delete;
    OpStat()
    : total_count(0),
      total_cost(0),
      total_put(0),
      total_get(0),
      total_delete(0)
    {
    }
};

struct SharedState
{
    std::mutex              mutex;
    std::condition_variable cond;
    int                     total;
    int                     initialized;
    int                     done;
    bool                    start;

    SharedState(int total)
    : total(total),
      initialized(0),
      done(0),
      start(false)
    {
    }
};

struct ThreadArg
{
    Benchmark*      bench;
    OpStat*         stat;
    SharedState*    shared;
    void (Benchmark::*method)(OpStat*);
};

struct BenchmarkOption
{
    int                     depth;
    int                     iosize;
    int                     threads;
    int                     interval;
    uint64_t                maxBlocks;
    bool                    verify;
    std::string             mode;
    std::vector<ThreadArg>  args;
    std::vector<OpStat>     stats;
    SharedState*            shared;
    BenchmarkOption()
    : depth(0),
      iosize(0),
      threads(0),
      maxBlocks(0),
      verify(false),
      shared(NULL)
    {
    }
};

class Benchmark
{
public:
    Benchmark();

    void Init(const BenchmarkOption& option);

    void Run();

    void Stop();

    void PrintConf();

    void PrintHeader();

    void PrintStat();

    void Get(OpStat* stat);

    void Put(OpStat* stat);

    void Delete(OpStat* stat);

    void Mixed(OpStat* stat);

    OpStat* GetStat(int i);

    ThreadArg* GetArg(int i);

private:
    KVClient*                mClient;
    std::vector<RawServerId> mRawServers;
    BenchmarkOption          mOption;
    bool                     mStop;
    SharedState*             mShardState;
};

}

#endif  // __BENCHMARK_H__