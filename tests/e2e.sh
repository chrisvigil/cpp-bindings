#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS)

set -euo pipefail
# Bazel rootpaths are relative to the test's runfiles working directory. The
# harness starts subprocesses, so pass absolute paths to all native consumers.
export FAULT_FFI_C_CONSUMER="$(realpath "$2")"
export FAULT_FFI_CPP_CONSUMER="$(realpath "$3")"
export FAULT_FFI_EXAMPLE="$(realpath "$4")"
export FAULT_FFI_SHARED_LIBRARY="$(realpath "$5")"
exec "$1" --exact native_consumers_and_ipc_scenarios --nocapture
