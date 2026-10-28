# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS)

load("@rules_cc//cc:defs.bzl", "cc_test")
load("@rules_shell//shell:sh_test.bzl", "sh_test")

cc_test(
    name = "consumer",
    size = "small",
    srcs = ["consumer.cpp"],
    copts = [
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        "-pthread",
    ],
    linkopts = ["-pthread"],
    deps = ["//:fault_reporter"],
)

sh_test(
    name = "e2e",
    size = "small",
    srcs = ["e2e.sh"],
    args = [
        "$(rootpath @score_fault_lib//src/fault_ffi:e2e)",
        "$(rootpath @score_fault_lib//src/fault_ffi:c_consumer)",
        "$(rootpath :consumer)",
        "$(rootpath //examples/cpp_reporter)",
        "$(rootpath @score_fault_lib//src/fault_ffi:fault_ffi_shared)",
    ],
    data = [
        ":consumer",
        "//examples/cpp_reporter",
        "@score_fault_lib//src/fault_ffi:c_consumer",
        "@score_fault_lib//src/fault_ffi:e2e",
        "@score_fault_lib//src/fault_ffi:fault_ffi_shared",
    ],
    tags = [
        "exclusive",
        "requires-linux",
    ],
)
