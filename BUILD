load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "raft_proto_internal",
    srcs = ["src/proto/raft.proto"],
)

proto_library(
    name = "kv_proto_internal",
    srcs = ["src/proto/kv.proto"],
)

cc_proto_library(
    name = "raft_proto",
    deps = [":raft_proto_internal"]
)

cc_proto_library(
    name = "kv_proto",
    deps = [":kv_proto_internal"]
)

cc_library(
    name = "raft_service",
    srcs = [
            "src/raft_service_impl.cpp",
            "src/kv_service_impl.cpp",
            "src/consensus_node_impl.cpp",
            "src/raft_closure.cpp",
            "src/util.cpp",
    ],
    hdrs = [ 
            "src/include/consensus_node.h",
            "src/include/raft_types.h",
            "src/include/status.h",
            "src/include/util.h",
            "src/raft_service_impl.h",
            "src/kv_service_impl.h",
            "src/consensus_node_impl.h",
            "src/raft_closure.h"
    ],
    deps = [ 
		":raft_proto",
        ":kv_proto",
		"@com_github_brpc_brpc//:brpc",
		"@com_github_google_glog//:glog",
		"@com_google_protobuf//:protobuf",
		"@com_github_gflags_gflags//:gflags",
		"@zlib//:zlib",
	],
)

cc_binary(
    name = "raft_server",
    srcs = [ "src/main.cpp" ],
    deps = [
        ":raft_proto",
        ":kv_proto",
        ":raft_service",
    ],
)

cc_library(
    name = "kv_client",
    srcs = [
                "src/client/kv_client_impl.cpp",
    ],
    hdrs = [
                "src/include/kv_client.h",
                "src/client/kv_client_impl.h",
                "src/include/status.h",
                "src/include/slice.h"
    ],
    deps = [
        ":raft_proto",
        ":kv_proto",
        ":raft_service",
    ],
)
