load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "raft_proto_internel",
    srcs = ["src/proto/raft.proto"],
)

cc_proto_library(
    name = "raft_proto",
    deps = [":raft_proto_internel"]
)

cc_library(
    name = "raft_service",
    srcs = [ "src/raft_service_impl.cpp", "src/consensus_node.cpp", "src/util.cpp" ],
    hdrs = [ "src/raft_service_impl.h", "src/util.h", "src/consensus_node.h" ],
    deps = [ 
		":raft_proto",
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
        ":raft_service",
        ],
)
