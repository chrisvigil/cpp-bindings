<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS) -->

# Decision: provider-owned C ABI, downstream C++17 bindings

## Evidence

Inspected `fault-lib` at `3e642d9` (the local `feat/cpp_abi` branch, based on
upstream `main` at `12dac50`) and `cpp-bindings` at `0a2313f`. Upstream repository
contents were also checked: `cpp-bindings/main` contains only its README, and
`fault-lib/main` does not contain the new ABI. There is no established C++ build
system, Rust bridge, or alternative interop convention in `cpp-bindings` to adopt.
Its declared purpose is application-facing C++ APIs and corresponding bindings
to OpenSOVD core components.

The implementation falls into these layers:

| Layer | Existing implementation | Owner |
| --- | --- | --- |
| Reporter behavior | `fault_lib::FaultApi`, `Reporter`, catalog, debounce, sink, IPC worker | `fault-lib` |
| ABI types and exports | `fault_ffi/src/types.rs`, `boundary.rs`, generated `fault_ffi.h` | `fault-lib` |
| ABI adaptation | `model.rs`: enum/status conversion, input limits, metadata merging | `fault-lib` |
| C++ convenience | `fault_ffi.hpp`: `string_view`, scoped stages, move-only RAII, string retrieval | `cpp-bindings` |
| C++ application usage | Native consumer and runnable reporter example | `cpp-bindings` |

`fault_ffi` uses public Rust APIs from **both** `fault_lib` and `common`, not Rust
private fields. Moving it is technically possible. However, it adapts Rust
error enums, fixed-capacity IPC containers, catalog construction, and singleton
lifetimes. Keeping it with those types allows atomic changes and regression tests.
Its generated C structs are deliberately distinct from the Rust IPC record types;
`repr(C)`/`ZeroCopySend` on IPC types alone would not provide a stable C API.

Cargo emits `rlib`, `staticlib`, and `cdylib`; Bazel already has equivalent Rust
library rules and a public `c_api` target. The ten `fl_*` exports use
`extern "C"` and `#[unsafe(no_mangle)]`. `cbindgen` 0.29.2 generates the C header.
No `cxx`/`autocxx` bridge is currently used. The wrapper calls only these C symbols.

## Alternatives

| Alternative | Benefits | Costs / decision |
| --- | --- | --- |
| **A. C ABI in `fault-lib`, C++ wrapper here** | ABI generation and Rust safety adaptation stay with the provider; C remains independent; C++ ownership matches this repository; one-way dependency on a versioned C surface | Coordinated releases and cross-repository integration tests are necessary. **Selected.** |
| **B. Move ABI and wrapper here** | All foreign-language entry points under one owner; feasible with a Rust bridge crate | Adds Cargo workspace/lockfile/toolchain management here; depends on unpublished `common` and `fault_lib` packages and their evolving Rust APIs; duplicates Bazel crate wiring and header generation ownership. Does not improve pointer/panic safety or eliminate Rust builds. No current repo convention justifies this cost. |
| **C1. Keep everything in `fault-lib`** | Simplest single-repository builds/tests and atomic changes | Technically sound, but gives the designated C++ application-bindings repository no ownership of its natural API. Splitting only the thin wrapper gives a clear responsibility boundary without relocating unsafe code. |
| **C2. Native C++ IPC implementation or `cxx`-only bridge** | Potentially idiomatic C++ transport integration | Native IPC duplicates protocol/catalog/debounce semantics and couples clients to Rust IPC layout. `cxx` alone does not preserve an independent C interface. Neither has evidence of benefit for this existing API. |

Dependency direction is `C++ app -> cpp-bindings -> fault-lib C ABI -> Rust
reporter/common -> iceoryx2`. There is no production or build-time reverse edge.
DFM is a test dependency of the C ABI crate, not of the C++ header or production
Rust ABI. The provider's test executable optionally accepts downstream binaries;
it does not fetch, compile, or include downstream C++ sources.

## Build and release implications

### S-Core applicability follow-up (2026-10-28)

The accepted [DR-002-Arch](https://eclipse-score.github.io/score/v0.7.0/design_decisions/DR-002-arch.html)
selects static-by-default, dynamic-by-reviewed-opt-in. Its cross-language
considerations support a C ABI in both modes; they do not mandate shared delivery.
The [ABI-compatible data types component](https://eclipse-score.github.io/reference_integration/main/modules/score_baselibs/baselibs/components/abi_compatible_data_types/docs/index.html)
specifies zero-copy IPC payloads. Its pointer/word-size/allocation restrictions do
not apply to these same-process borrowed arguments or opaque owners.

The accepted [OpenSOVD interface ADR](https://github.com/eclipse-opensovd/opensovd/blob/main/docs/design/adr/001-adr-score-interface.md)
identifies the Fault Library as the joint technical/organizational interface, but
does not assign the C ABI to a particular repository. The split selected here
follows the implementation dependencies and each repository's stated purpose.
[Issue #108](https://github.com/eclipse-opensovd/opensovd/issues/108) is still an
unconfirmed integration proposal; its latest fault-reporting/transport/adapter
ownership questions are unanswered. It does not require an HTTP reporter or a
`score::mw::diag::DTC` adapter in this wrapper.

The full source-linked requirement/gap assessment is maintained by the provider
in `fault-lib/src/fault_ffi/architecture.md`. Static remains the recommended build
default; the existing shared artifacts lack the complete opted-in release controls.
Neither this split nor panic containment establishes safety qualification.

### Existing build integration

Cargo remains entirely in `fault-lib`, including its existing lockfile and
unsafe-lint exception. C++ users can consume just prebuilt headers/libraries.
Bazel users consume `@opensovd_cpp_bindings//:fault_reporter`, which depends on
`@score_fault_lib//src/fault_ffi:c_api`. C++17 must be selected in the consuming
root; per-target `copts` and `.bazelrc` are not transitive language guarantees.

The new Bazel root must repeat `fault-lib`'s non-registry dependency overrides:
Bzlmod deliberately ignores overrides in dependency modules. This includes the
existing mutable `rules_boost` master archive, an inherited reproducibility
limitation. These overrides do not introduce a reverse dependency. Moving the
Rust bridge would also require resolving these dependencies, plus a separate
Cargo setup. Cargo and Bazel retain the provider's different Rust compilers
(`nightly-2025-07-14` and 1.91.0 respectively).
The downstream Bazel root explicitly selects the 1.91.0 Rust toolchain as well;
otherwise the default rules_rust compiler can take priority over the dependency's
toolchain and fail to compile its Rust 2024 let-chains.

The checked-in local override requires the paired checkout because neither the
ABI nor the updated test harness has an upstream release. Integration sequence:

1. Land the provider C ABI, its independent tests, and the exported test harness.
2. Pin that immutable upstream commit/release here instead of `local_path_override`.
3. Enable automatic upstream CI against the selected provider revision; coordinated
   CI can already run `tools/test_cargo.sh` and the Bazel tests with both checkouts.
4. Package the wrapper with the matching provider-generated C header and binary.
   Update the header and library together; retain the runtime ABI-major check.

No function, namespace, configuration layout, exported symbol, or status changes
are required. Include/dependency locations necessarily change. A forwarding
header or alias in `fault-lib` that depends on this repository would create a
cycle, so migration instructions replace those build paths explicitly.

## Changes by repository

In `fault-lib`, `src/fault_ffi/{src,include/fault_ffi.h,Cargo.toml,cbindgen.toml}`
remain the provider implementation. Its `BUILD`, C consumer, and `tests/e2e.rs`
now support independent C verification plus an optional downstream executable
hook. Root/ABI READMEs and `.github/workflows/build_test.yml` use the new ownership
and build paths.

In `cpp-bindings`, `include/fault_ffi.hpp`, `tests/consumer.cpp`, and
`examples/cpp_reporter/` are moved from the provider. The wrapper adds only
language/ABI-header compile-time checks. `BUILD`, `MODULE.bazel`, `.bazelrc`,
`.bazelversion`, `tests/BUILD`, `tests/e2e.sh`, and `tools/test_cargo.sh` provide
static/shared integration paths. Tests extend ownership/error/thread coverage;
README/docs describe the decision, contracts, build migration, and results.
LICENSE/NOTICE preserve the original source attribution.

## Boundary and API assessment

- Rust owns `Box` allocations behind opaque handles and destroys them through
  matching exports. `FlReporter` retains a `FaultApi` clone, which keeps the
  catalog, sink, and enabling-condition manager alive after context destruction.
- The underlying `OnceLock` permits one successful initialization per process.
  Failed catalog/transport initialization can be retried. Repository movement
  cannot change that policy; multiple statically embedded runtimes must be avoided.
  Keep a shared library loaded until process exit after any use: worker shutdown
  can detach after a timeout, failed initialization can also detach a worker,
  and calling threads may retain TLS destructors. Destroying handles is insufficient.
- Configs and spans borrow inputs for one call. Rust copies source/metadata/IDs;
  C++ configuration helpers merely hold borrowed views until that call. Destruction
  is not an acknowledgement or flush of the worker queue.
- Individual reporters have mutable debounce state. Callers must serialize use,
  move, and destruction; distinct reporters can be used independently. Rust's
  existing `Reporter: Send + Sync` tests and C++ foreign-thread integration checks
  support that contract, but arbitrary user data races are outside ABI validation.
- Boundary guards catch unwinding panics and map failures to stable status values.
  TLS diagnostics preserve calling-thread isolation; cleanup can clear them.
  `panic=abort`, allocator aborts, and invalid pointer access cannot be caught.
  Catching a panic contains unwinding, not a transactional rollback guarantee.
- The C++ wrapper uses status-returning `noexcept` operations for factories and
  publish, and nonthrowing ownership transfer/destruction. String-returning helpers
  allocate and may throw; the example's exception policy is application-level.
- Numeric, text, and raw UUID identifiers, lifecycle phase/stage, metadata defaults
  and overrides, Rust timestamps, catalog verification, and debounce are preserved.
  The existing C ABI is a reporter subset of the larger Rust API: monitors,
  callbacks, custom sinks/timestamps, and query APIs are not silently added here.
- C++14 cannot compile the existing `string_view` interface. C++17 is the explicit
  target; changing it to C++14 would unnecessarily change that interface. This is
  language-version alignment with S-Core/MISRA C++:2023, not a verified MISRA
  compliance claim (allocation, pointer casts, and exception policy need review).

## Validation design

Provider tests cover conversions, capacities, invalid UTF-8/pointers-as-null,
error mappings, panic containment, TLS, initialization retry/one-shot behavior,
context-before-reporter destruction, and actual C reporting to DFM. A real C11
consumer compiles and links without any C++ header. The generated header is checked
with the pinned generator. The follow-up adds a frozen C v1 declaration/layout/value
baseline and checks the shared library's external exports in Cargo and Bazel CI.
The generated header now carries the corrected lifetime/thread/panic contracts;
its declarations and values are preserved.

Downstream tests preserve live-handle move/assignment and all identifier forms,
and check failed factories, catalog strings, foreign-thread diagnostics and
exclusive reporter transfer. Shared-link Cargo/native and static-link Bazel
paths use the same consumers. Integration polls DFM for fault state and metadata,
including example recovery. Fresh subprocesses preserve singleton/KVS constraints;
the default IPC service namespace requires serial execution across suites.

See [validation results](validation.md) for the checks actually run and remaining
environment/integration limitations.
