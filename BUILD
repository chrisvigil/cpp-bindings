# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS)

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "fault_reporter",
    hdrs = ["include/fault_ffi.hpp"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = ["@score_fault_lib//src/fault_ffi:c_api"],
)
