#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS)

set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
fault_lib="$(cd "${FAULT_LIB_ROOT:-$root/../fault-lib}" && pwd)"
mkdir -p "${CPP_BINDINGS_BUILD_DIR:-$root/build/cargo}"
build="$(cd "${CPP_BINDINGS_BUILD_DIR:-$root/build/cargo}" && pwd)"

# Run inside the dependency so Cargo selects its pinned Rust toolchain.
cd "$fault_lib"
cargo build --locked -p fault_ffi
target="$(cargo metadata --locked --format-version 1 --no-deps |
    python3 -c 'import json, sys; print(json.load(sys.stdin)["target_directory"])')"
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -pthread
    -I"$root/include" -I"$fault_lib/src/fault_ffi/include")
link=(-L"$target/debug" -Wl,-rpath,"$target/debug" -lfault_ffi)
"${CXX:-c++}" "${flags[@]}" "$root/tests/consumer.cpp" "${link[@]}" -o "$build/consumer"
"${CXX:-c++}" "${flags[@]}" "$root/examples/cpp_reporter/main.cpp" "${link[@]}" -o "$build/cpp_reporter"
export FAULT_FFI_CPP_CONSUMER="$build/consumer"
export FAULT_FFI_EXAMPLE="$build/cpp_reporter"
cargo test --locked -p fault_ffi --test e2e -- --nocapture
