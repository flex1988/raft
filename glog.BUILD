cc_library(
    name = "glog",
    srcs = [
        "src/base/commandlineflags.h",
        "src/base/googleinit.h",
        "src/demangle.cc",
        "src/logging.cc",
        "src/raw_logging.cc",
        "src/symbolize.cc",
        "src/utilities.cc",
        "src/vlog_is_on.cc",
    ],
    hdrs = [
        "raw_logging_h",
        "src/base/mutex.h",
        "src/demangle.h",
        "src/symbolize.h",
        "src/utilities.h",
        "src/glog/log_severity.h",
        ":config_h",
        ":logging_h",
        ":stl_logging_h",
        ":vlog_is_on_h",
    ],
    copts = [
        # Disable warnings that exists in glog
        "-Wno-sign-compare",
        "-Wno-unused-local-typedefs",
        # Inject google namespace as "google"
        "-D_START_GOOGLE_NAMESPACE_='namespace google {'",
        "-D_END_GOOGLE_NAMESPACE_='}'",
        "-DGOOGLE_NAMESPACE='google'",
        # Allows src/base/mutex.h to include pthread.h.
        "-DHAVE_PTHREAD",
        # Allows src/logging.cc to determine the host name.
        "-DHAVE_SYS_UTSNAME_H",
        # System header files enabler for src/utilities.cc
        # Enable system calls from syscall.h
        "-DHAVE_SYS_SYSCALL_H",
        # Enable system calls from sys/time.h
        "-DHAVE_SYS_TIME_H",
        "-DHAVE_STDINT_H",
        "-DHAVE_STRING_H",
        # For logging.cc
        "-DHAVE_PREAD",
        "-DHAVE_FCNTL",
        "-DHAVE_SYS_TYPES_H",
        # Allows syslog support
        "-DHAVE_SYSLOG_H",
        # GFlags
        "-isystem $(GENDIR)/external/com_github_gflags_gflags/",
        "-DHAVE_LIB_GFLAGS",
        # Necessary for creating soft links of log files
        "-DHAVE_UNISTD_H",
    ],
    includes = [
        ".",
        "src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//external:gflags",
    ],
)

# Below are the generation rules that generates the necessary header
# files for glog. Originally they are generated by CMAKE
# configure_file() command, which replaces certain template
# placeholders in the .in files with provided values.

# gen_sh is a bash script that provides the values for generated
# header files. Under the hood it is just a wrapper over sed.
genrule(
    name = "gen_sh",
    outs = [
        "gen.sh",
    ],
    cmd = """
cat > $@ <<"EOF"
#! /bin/sh
sed -e 's/@ac_cv_have_unistd_h@/1/g' \
    -e 's/@ac_cv_have_stdint_h@/1/g' \
    -e 's/@ac_cv_have_systypes_h@/1/g' \
    -e 's/@ac_cv_have_libgflags_h@/1/g' \
    -e 's/@ac_cv_have_uint16_t@/1/g' \
    -e 's/@ac_cv_have___builtin_expect@/1/g' \
    -e 's/@ac_cv_have_.*@/0/g' \
    -e 's/@ac_google_start_namespace@/namespace google {/g' \
    -e 's/@ac_google_end_namespace@/}/g' \
    -e 's/@ac_google_namespace@/google/g' \
    -e 's/@ac_cv___attribute___noinline@/__attribute__((noinline))/g' \
    -e 's/@ac_cv___attribute___noreturn@/__attribute__((noreturn))/g' \
    -e 's/@ac_cv___attribute___printf_4_5@/__attribute__((__format__ (__printf__, 4, 5)))/g'
EOF""",
)

genrule(
    name = "config_h",
    srcs = [
        "src/config.h.cmake.in",
    ],
    outs = [
        "config.h",
    ],
    cmd = "awk '{ gsub(/^#cmakedefine/, \"//cmakedefine\"); print; }' $(<) > $(@)",
)

genrule(
    name = "logging_h",
    srcs = [
        "src/glog/logging.h.in",
    ],
    outs = [
        "glog/logging.h",
    ],
    cmd = "$(location :gen_sh) < $(<) > $(@)",
    tools = [":gen_sh"],
)

genrule(
    name = "raw_logging_h",
    srcs = [
        "src/glog/raw_logging.h.in",
    ],
    outs = [
        "glog/raw_logging.h",
    ],
    cmd = "$(location :gen_sh) < $(<) > $(@)",
    tools = [":gen_sh"],
)

genrule(
    name = "stl_logging_h",
    srcs = [
        "src/glog/stl_logging.h.in",
    ],
    outs = [
        "glog/stl_logging.h",
    ],
    cmd = "$(location :gen_sh) < $(<) > $(@)",
    tools = [":gen_sh"],
)

genrule(
    name = "vlog_is_on_h",
    srcs = [
        "src/glog/vlog_is_on.h.in",
    ],
    outs = [
        "glog/vlog_is_on.h",
    ],
    cmd = "$(location :gen_sh) < $(<) > $(@)",
    tools = [":gen_sh"],
)
