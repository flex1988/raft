workspace(name = "raft")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_google_protobuf",
    strip_prefix = "protobuf-3.6.1.3",
    sha256 = "9510dd2afc29e7245e9e884336f848c8a6600a14ae726adb6befdb4f786f0be2",
    type = "zip",
    url = "https://github.com/protocolbuffers/protobuf/archive/v3.6.1.3.zip",
)

http_archive(
    name = "com_github_google_glog",
    build_file = "//:glog.BUILD",
    strip_prefix = "glog-a6a166db069520dbbd653c97c2e5b12e08a8bb26",
    url = "https://github.com/google/glog/archive/a6a166db069520dbbd653c97c2e5b12e08a8bb26.tar.gz"
)

http_archive(
    name = "com_github_gflags_gflags",
    strip_prefix = "gflags-46f73f88b18aee341538c0dfc22b1710a6abedef",
    url = "https://github.com/gflags/gflags/archive/46f73f88b18aee341538c0dfc22b1710a6abedef.tar.gz",
)

http_archive(
    name = "com_github_google_leveldb",
    build_file = "//:leveldb.BUILD",
    strip_prefix = "leveldb-a53934a3ae1244679f812d998a4f16f2c7f309a6",
    url = "https://github.com/google/leveldb/archive/a53934a3ae1244679f812d998a4f16f2c7f309a6.tar.gz"
)

new_local_repository(
    name = "openssl",
    path = "/usr",
    build_file = "//:openssl.BUILD",
)

bind(
    name = "ssl",
    actual = "@openssl//:ssl"
)

new_local_repository(
    name = "zlib",
    build_file = "//:zlib.BUILD",
    path = "/usr",
)

http_archive(
    name = "rules_proto",
    sha256 = "66bfdf8782796239d3875d37e7de19b1d94301e8972b3cbd2446b332429b4df1",
    strip_prefix = "rules_proto-4.0.0",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0.tar.gz",
        "https://github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0.tar.gz",
    ],
)

http_archive(
    name = "com_github_brpc_brpc",
    strip_prefix = "brpc-1.3.0",
    url = "https://github.com/apache/brpc/archive/refs/tags/1.3.0.tar.gz"
)

http_archive(
    name = "com_github_madler_zlib",
    strip_prefix = "zlib-1.2.13",
    build_file = "//:madler_zlib.BUILD",
    url = "https://github.com/madler/zlib/archive/refs/tags/v1.2.13.tar.gz"
)

http_archive(
    name = "com_google_googletest",
    strip_prefix = "googletest-0fe96607d85cf3a25ac40da369db62bbee2939a5",
    url = "https://github.com/google/googletest/archive/0fe96607d85cf3a25ac40da369db62bbee2939a5.tar.gz",
)

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
rules_proto_dependencies()
rules_proto_toolchains()
