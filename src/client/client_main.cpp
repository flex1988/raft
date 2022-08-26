#include <gflags/gflags.h>
#include <brpc/channel.h>
#include "src/proto/kv.pb.h"

DEFINE_string(server, "127.0.0.1:8000", "IP Address of server");

int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Channel channel;

    brpc::ChannelOptions options;
    options.timeout_ms = 100; // 100ms
    if (channel.Init(FLAGS_server.c_str(), "", &options) != 0)
    {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    raft::KVService_Stub stub(&channel);

    while (!brpc::IsAskedToQuit())
    {
        raft::GetRequest request;
        raft::GetResponse response;
        brpc::Controller ctrl;
        
        request.set_key("hello");
        stub.Get(&ctrl, &request, &response, NULL);
        if (ctrl.Failed())
        {
            LOG(ERROR) << "Received response from " << ctrl.remote_side() << " to " << ctrl.local_side() << " : retcode " << response.retcode() << " Error: " << ctrl.ErrorText();
        }
        else
        {
            LOG(INFO) << "Received response from " << ctrl.remote_side() << " to " << ctrl.local_side() << "Key: " << request.key() << " Val: " << response.val();
        }
    }

    return 0;
}