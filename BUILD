load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "libraft_proto_internal",
    srcs = ["src/proto/raft.proto", "src/proto/kv.proto", "src/proto/common.proto"],
)

cc_proto_library(
    name = "libraft_proto",
    deps = [":libraft_proto_internal"]
)

cc_library(
    name = "libraft",
    srcs = [
            "src/raft_service_impl.cpp",
            "src/kv_service_impl.cpp",
            "src/raft_closure.cpp",
            "src/consensus_node_impl.cpp",
            "src/util.cpp",
            "src/raft_impl.cpp",
            "src/progress_tracker.cpp",
	        "src/progress.cpp",
            "src/raft_status.cpp",
            "src/memory_storage.cpp",
            "src/raft_unstable_log.cpp",
            "src/raft_log.cpp"
    ],
    hdrs = [ 
            "src/include/raft_types.h",
            "src/include/status.h",
            "src/include/util.h",
            "src/include/raft.h",
            "src/raft_service_impl.h",
            "src/kv_service_impl.h",
            "src/consensus_node_impl.h",
            "src/raft_closure.h",
            "src/raft_impl.h",
            "src/progress_tracker.h",
	        "src/progress.h",
            "src/memory_storage.h",
            "src/raft_unstable_log.h",
            "src/raft_log.h"
    ],
    deps = [ 
		":libraft_proto",
		"@com_github_google_glog//:glog",
		"@com_github_brpc_brpc//:brpc",
		"@com_google_protobuf//:protobuf",
		"@com_github_gflags_gflags//:gflags",
		"@zlib//:zlib",
	],
)

cc_binary(
    name = "raft_server",
    srcs = [ "src/main.cpp" ],
    deps = [
        ":libraft_proto",
        ":libraft",
    ],
)

cc_binary(
    name = "raft_unittest",
    srcs = [
        "src/unittest/raft_unittest.cpp",
        "src/unittest/raft_paper_unittest.cpp",
        "src/unittest/unittest_main.cpp",
        "src/unittest/raft_unittest_util.cpp",
        "src/unittest/storage_unittest.cpp",
        "src/unittest/unstable_log_unittest.cpp",
        "src/unittest/log_unittest.cpp",
        "src/unittest/raft_unittest_util.h"
    ],

    deps = [
        ":libraft",
        "@com_google_googletest//:gtest",
    ]
)

cc_library(
    name = "libkv_client",
    srcs = [
            "src/client/kv_client_impl.cpp",
    ],
    hdrs = [
            "src/include/kv_client.h",
            "src/client/kv_client_impl.h",
            "src/include/status.h",
            "src/include/slice.h",
    ],
    deps = [
        ":libraft_proto",
        ":libraft",
    ],
)

cc_library(
    name = "libkv_benchmark",
    srcs = [
        "src/benchmark/benchmark.cpp",
    ],
    hdrs = [ "src/benchmark/benchmark.h" ],
    deps = [
        ":libraft_proto",
        ":libraft",
        ":libkv_client"
    ],
)

cc_binary(
    name = "kv_benchmark",
    srcs = [ "src/benchmark/benchmark.cpp" ],
    deps = [
        ":libkv_benchmark",
    ],
)
