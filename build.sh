bazel build //:raft_server --incompatible_disable_deprecated_attr_params=false --incompatible_new_actions_api=false --verbose_failures --sandbox_debug --copt -DHAVE_ZLIB=1
