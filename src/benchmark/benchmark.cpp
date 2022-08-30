#include <unistd.h>
#include <thread>

#include "gflags/gflags.h"
#include "src/include/util.h"
#include "src/benchmark/benchmark.h"

DEFINE_int32(runtime, 60, "default benchmark runtime");
DEFINE_string(mode, "put", "op mode");
DEFINE_string(servers, "", "server address, for example `127.0.0.1:8000,127.0.0.1:8001,127.0.0.1:8002`");

namespace raft
{

Benchmark::Benchmark()
: mClient(NULL),
  mStop(false),
  mShardState(NULL)
{
}

void Benchmark::Get(OpStat* stat)
{
    do
    {
        std::string k = "hello";
        std::string v;
        mClient->Get(k, &v, NULL);
        stat->total_get++;
    } while (!mStop);
}

void Benchmark::Put(OpStat* stat)
{
    do
    {
        std::string k = "hello";
        std::string v = "world!";
        mClient->Put(k, v, NULL);
        stat->total_put++;
    } while (!mStop);
}

void Benchmark::Init(const BenchmarkOption& option)
{
    mOption = option;
    mOption.args.resize(option.threads);
    mOption.stats.resize(option.threads);

    for (int i = 0; i < option.threads; i++)
    {
        mOption.args[i].bench = this;
        if (option.mode == "get")
        {
            mOption.args[i].method = &raft::Benchmark::Get;
        }
        else if (option.mode == "put")
        {
            mOption.args[i].method = &raft::Benchmark::Put;
        }
        mOption.args[i].stat = &mOption.stats[i];
        mOption.args[i].shared = option.shared;
    }


    std::vector<std::string> servers;
    raft::SplitString(servers, FLAGS_servers, ",");
    KVClientOptions kvOptions;
    for (int i = 0; i < servers.size(); i++)
    {
        fprintf(stdout, "add server %s\n", servers[i].c_str());
        butil::EndPoint endpoint;
        assert(butil::str2endpoint(servers[i].c_str(), &endpoint) == 0);
        kvOptions.servers.push_back(endpoint);
        int pos = servers[i].find(":");
        std::string ip = servers[i].substr(0, pos);
        int port = atoi(servers[i].substr(pos + 1).c_str());
        RawServerId id(ip, port);
        kvOptions.serverIds.push_back(id);
    }

    mClient = new KVClientImpl;
    assert(mClient->Open(kvOptions).IsOK());
}

void Benchmark::Run()
{
    fprintf(stdout, "raft kv benchmark started\n");
    PrintConf();
    PrintHeader();
    for (int i = 0; i < FLAGS_runtime; i++)
    {
        sleep(1);
        PrintStat();
    }
}

void Benchmark::PrintConf()
{
    fprintf(stdout, "%8s\t%8s\n", "Mode", mOption.mode.c_str());
    fprintf(stdout, "%8s\t%8d\n", "Threads", mOption.threads);
    fprintf(stdout, "\n\n");
}

void Benchmark::PrintHeader()
{
    fprintf(stdout, "%14s\t%14s\t%14s\t%16s\n\n", "get", "put", "delete", "latency");
}

void Benchmark::PrintStat()
{
    uint64_t getCount = 0;
    uint64_t putCount = 0;
    uint64_t deleteCount = 0;

    for (uint32_t i = 0; i < mOption.stats.size(); i++)
    {
        getCount += mOption.stats[i].total_get;
        putCount += mOption.stats[i].total_put;
        deleteCount += mOption.stats[i].total_delete;
    }
    memset(&mOption.stats[0], 0, sizeof(OpStat) * mOption.stats.size());
    uint64_t total = getCount + putCount + deleteCount;

    if (total == 0)
    {
        fprintf(stdout, "%14ld\t%14ld\t%14ld\t%14ldus\n", 0UL, 0UL, 0UL, 0UL);
    }
    else
    {
        fprintf(stdout, "%14ld\t%14ld\t%14ld\t%14ldus\n", getCount, putCount, deleteCount, mOption.interval * 1000000 / total);
    }
}

void Benchmark::Stop()
{
    mStop = true;
    fprintf(stdout, "raft kv benchmark stopped\n");
}

ThreadArg* Benchmark::GetArg(int i)
{
    return &mOption.args[i];
}

static void ThreadMain(void* a)
{
    raft::ThreadArg* arg = reinterpret_cast<raft::ThreadArg*>(a);

    (arg->bench->*arg->method)(arg->stat);
}

}

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);

    raft::SharedState shared(1);

    raft::BenchmarkOption option;
    option.threads = 1;
    option.interval = 1;
    option.shared = &shared;
    option.mode = FLAGS_mode;

    raft::Benchmark bench;
    bench.Init(option);

    std::thread t1(raft::ThreadMain, bench.GetArg(0));

    bench.Run();
    bench.Stop();

    t1.join();

    return 0;
}