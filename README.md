<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS) -->

# Eclipse OpenSOVD C++ bindings

C++ libraries providing application APIs and binding implementations for OpenSOVD
core components such as the Server and Fault Manager.

## Fault reporter (C++17)

[`include/fault_ffi.hpp`](include/fault_ffi.hpp) provides the existing `fault::Context`,
`fault::Reporter`, configuration/metadata helpers, and lifecycle `fault::Stage`.
It is a header-only, move-only RAII wrapper over the independent C ABI provided by
[`fault-lib`](https://github.com/eclipse-opensovd/fault-lib).
The C header and Rust shared/static libraries are built and versioned there.

```text
C++ application -> cpp-bindings (fault_ffi.hpp)
                                |
C application ----------------> fault-lib (fault_ffi.h / libfault_ffi)
                                           -> Rust reporter -> iceoryx2 -> DFM
```

See the [architecture decision](docs/fault-reporter-architecture.md) and
[runnable reporter example](examples/cpp_reporter/README.md).

### Build and test

Current integration pins the matching immutable `fault-lib` C ABI revision.
Upstream `fault-lib/main` does not yet contain this ABI, so this branch points
to the internally developed fork commit until the ABI is reviewed and merged.
No sibling checkout, Rust bridge crate, or copied C header is required.

On Linux, with Bazel **8.3.0** (Bazelisk honors `.bazelversion`):

```sh
bazel build //:fault_reporter //examples/cpp_reporter
bazel test //tests:consumer //tests:e2e
```

For local fault-lib development, a consumer root may temporarily override the
`score_fault_lib` module with a checkout path; that override belongs in the
consumer root and is not required by this module.
The public Bazel dependency is `@opensovd_cpp_bindings//:fault_reporter`.
Consumers must select C++17 themselves; this repository's `.bazelrc` does not
propagate to a consuming root module. Root modules must also supply the
non-registry overrides currently required by `fault-lib`; see `MODULE.bazel`.
The Bazel target links the Rust static library and its native dependencies.
This is also the recommended S-Core delivery default. Shared-library testing
below does not establish a qualified S-Core dynamic-delivery release; remaining
requirements are recorded in `fault-lib/src/fault_ffi/architecture.md`.

Alternatively, build with Cargo and the native C++ compiler, then test shared
linking and actual delivery through DFM:

```sh
bash tools/test_cargo.sh
```

Prerequisites: `fault-lib`'s pinned Rust toolchain, `cc`, `c++`, GNU `nm`
(binutils), Python 3, and Linux iceoryx2 runtime support.
Overrides: `FAULT_LIB_ROOT`, `CXX`, `CC`,
`CPP_BINDINGS_BUILD_DIR`; compiler overrides are executable names/paths, not
command strings. Cargo's target directory is discovered from metadata.
Run IPC tests without an external DFM and sequentially with other suites using
the same default service names. Bazel's integration target is `exclusive`.

For a prebuilt SDK, ship `fault_ffi.hpp` from this repository alongside the
matching generated `fault_ffi.h` and `libfault_ffi` from `fault-lib`. Add both
include directories and link `-lfault_ffi`; shared linking also needs a runtime
loader path. C applications need only the C header/library. Static-link system
libraries are listed by `cargo rustc --locked -p fault_ffi --release -- --print
native-static-libs` in `fault-lib`.

### Compatibility and lifetime contracts

- Existing `#include "fault_ffi.hpp"`, `fault::` names, parameters, and status
  codes are preserved. Replace the old `fault-lib` `:cpp_api` Bazel dependency
  and add this repository's include path. The C symbols/layouts remain ABI v1.
- C++17 is required (including `std::string_view`); C++14 is intentionally not
  supported. This targets the S-Core language requirement and MISRA C++:2023's
  language baseline. A MISRA compliance assessment has not been performed.
- Check `fl_abi_version() == FL_ABI_VERSION` before creating handles. The header
  also checks that it is being compiled against ABI-major-1 declarations.
- Input views and metadata arrays are borrowed until creation/publication
  returns. Rust copies them; a configuration helper does not own its strings.
- Context and reporter objects are noncopyable. Move/reset/destruction consumes
  each handle once through the C API. A reporter may outlive its context.
  Failed factory calls preserve the output object; successful ones replace it.
- Initialization succeeds once per process, even after all objects are reset.
  Link one copy of the Rust reporter runtime per process; do not statically
  embed it independently into multiple plugins. After any use, keep a shared
  library loaded until process exit, even after failed creation or all resets;
  worker teardown and thread-local destructors can outlive calls/handles.
- Serialize operations, moves, and destruction on each individual handle.
  Independent reporters can operate on separate threads. There is no hidden
  wrapper mutex. Diagnostics are thread-local and must be captured on the
  calling thread **before cleanup**, which may clear them.
- Factory/publish/move/reset operations and destructors are `noexcept` and use
  C status codes. `last_error()` and `catalog_id(std::string&)` allocate and may
  throw C++ allocation exceptions; the example additionally throws on status
  failures. Rust unwinding panics are contained at the C boundary, but aborts,
  OOM, invalid pointers, or data races are not recoverable status errors.
  Containment does not guarantee rollback, worker recovery, or freedom from
  interference. Calls may allocate/block and are not async-signal-safe.
- `FL_OK` means local acceptance or debounce suppression. Destruction is not a
  delivery flush. The integration harness polls DFM to verify actual delivery.

Full capacities, ASCII restrictions, statuses, and pointer contracts are defined
by `fault-lib/src/fault_ffi/README.md` and its generated C header. Enabling-condition
callbacks, custom timestamps, and DFM queries remain outside the reporter ABI.

## Verification ownership

`fault-lib` owns C ABI unit tests, C11 compilation/linking, malformed-input and
panic/TLS tests, and DFM integration fixtures. Its tests run independently.
This repository owns C++17 compilation, RAII/move and foreign-thread checks,
and the example. The downstream runner passes compiled executables into the
provider's conformance harness, which checks failed/recovered faults and metadata.
No dependency from `fault-lib` back to this repository is introduced.
