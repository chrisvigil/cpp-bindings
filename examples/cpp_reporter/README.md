<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS) -->

# C++17 fault reporter

Run commands from the `cpp-bindings` root unless noted otherwise. Use a sibling
`../fault-lib` checkout with the C ABI changes, its pinned Rust toolchain, a
C++17 compiler, and the existing iceoryx2 runtime requirements.

Build the shared Rust library and the C++ application:

```sh
(cd ../fault-lib && cargo build --locked -p fault_ffi --release)
mkdir -p build
c++ -o build/cpp_reporter -std=c++17 -Wall -Wextra -Werror -pedantic -pthread \
    examples/cpp_reporter/main.cpp -Iinclude -I../fault-lib/src/fault_ffi/include \
    -L../fault-lib/target/release -lfault_ffi -Wl,-rpath,"$PWD/../fault-lib/target/release"
```

Start DFM from the `fault-lib` root in one terminal (choose a persistent storage directory):

```sh
cargo run --locked -p dfm_bin -- \
    --catalog-dir src/fault_lib/tests/data --storage-dir target/cpp-dfm-storage
```

After `DFM ready`, run in another terminal:

```sh
build/cpp_reporter ../fault-lib/src/fault_lib/tests/data/hvac_fault_catalog.json
```

The application reads the catalog, creates a reporter, releases its context,
then reports `Failed -> PrePassed -> Passed` with source identity and metadata.
It uses `hvac.blower.speed_sensor_mismatch`, which has no reporter debounce. The
numeric HVAC example has a 60-second hold time and is unsuitable for a quick demo.
Demo recovery events are spaced by 200ms for this fault's 100ms manager cooldown.

To leave the fault failed, or report recovery in another process:

```sh
build/cpp_reporter ../fault-lib/src/fault_lib/tests/data/hvac_fault_catalog.json hvac.blower.speed_sensor_mismatch failed
build/cpp_reporter ../fault-lib/src/fault_lib/tests/data/hvac_fault_catalog.json hvac.blower.speed_sensor_mismatch passed
```

With Bazel, build/run the statically linked equivalent:

```sh
bazel build //examples/cpp_reporter
bazel run //examples/cpp_reporter -- "$PWD/../fault-lib/src/fault_lib/tests/data/hvac_fault_catalog.json"
```

Successful publication is asynchronous local acceptance, not confirmation from
DFM; reporter debounce can also suppress records. The program does not claim
to flush on exit. Automated ABI tests query DFM to verify eventual state.
See [binding contracts and integration guidance](../../README.md).
