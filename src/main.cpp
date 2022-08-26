#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include "src/raft_service_impl.h"
#include "src/kv_service_impl.h"
#include "src/include/util.h"

DEFINE_int32(port, 8000, "server port");
DEFINE_string(listen_addr, "127.0.0.1", "server listen address");
DEFINE_int32(idle_timeout_s, -1, "connection idle_timeout_s");
DEFINE_int32(request_vote_timeout_ms, 1000, "request vote timeout ms");
DEFINE_int32(election_timeout_ms_min, 1500, "election timeout ms");
DEFINE_int32(election_timeout_ms_max, 3000, "election timeout ms");
DEFINE_int32(heartbeat_timeout_ms, 500, "heart beat interval ms");
DEFINE_string(servers, "", "");

int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;

    raft::ConsensusNodeImpl node;
    if (FLAGS_servers.empty())
    {
        LOG(ERROR) << "no given cluster servers, exit now";
        return -1;
    }
    std::vector<std::string> servers;
    raft::SplitString(servers, FLAGS_servers, ",");
    raft::ServerId id;
    id.set_ip(FLAGS_listen_addr);
    id.set_port(FLAGS_port);
    node.SetOwnId(id);
    for (uint32_t i = 0; i < servers.size(); i++)
    {
        int pos = servers[i].find(":");
        std::string ip = servers[i].substr(0, pos);
        int port = atoi(servers[i].substr(pos + 1).c_str());
        raft::ServerId peer;
        peer.set_ip(ip);
        peer.set_port(port);
        node.AddPeer(peer);
    }
    node.Start(0);

    raft::RaftServiceImpl raft(&node);
    if (server.AddService(&raft, brpc::SERVER_DOESNT_OWN_SERVICE) != 0)
    {
        LOG(ERROR) << "add raft service failed";
        return -1;
    }

    raft::KVServiceImpl kv(&node);
    if (server.AddService(&kv, brpc::SERVER_DOESNT_OWN_SERVICE) != 0)
    {
        LOG(ERROR) << "add kv service failed";
        return -1;
    }

    butil::EndPoint point;
    if (!FLAGS_listen_addr.empty())
    {
        if (butil::str2endpoint(FLAGS_listen_addr.c_str(), FLAGS_port, &point) < 0)
        {
            LOG(ERROR) << "Invalid listen address:" << FLAGS_listen_addr;
            return -1;
        }
    }
    else
    {
        point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
    }
    brpc::ServerOptions option;
    option.idle_timeout_sec = FLAGS_idle_timeout_s;
    if (server.Start(point, &option) != 0)
    {
        LOG(ERROR) << "failed to start raft server";
        return -1;
    }

    server.RunUntilAskedToQuit();
    return 0;
}
