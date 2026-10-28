<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS) -->

# Reporter split and architecture review: validation results

Validated on Linux x86-64 with GCC/G++ 11.4.0, Cargo's pinned
`nightly-2025-07-14`, Bazel 8.3.0, and Bazel's Rust 1.91.0. In this environment,
`USE_BAZEL_VERSION=8.3.0` was set explicitly because the ambient Bazel launcher
otherwise selected 9.3.1. IPC suites were run sequentially across repositories.

## Passing checks

From `fault-lib/`:

| Command / check | Result |
| --- | --- |
| `cargo build --locked -p fault_ffi --release` | Static and shared libraries built |
| `cargo test --locked -p fault_ffi` | 11 unit tests and 2 integration test entries passed; C-only native/IPC scenarios completed |
| `cargo test --locked --workspace --all-targets --features fault_lib/testutils` | All 407 tests passed at the split stage, before the two additional boundary tests; includes real IPC and compilation of the feature-gated example |
| `cargo clippy --locked -p fault_ffi --all-targets -- -D warnings` | Passed |
| `cargo fmt --all -- --check` | Passed |
| `bash src/fault_ffi/tools/generate_header.sh --check` | Passed with cbindgen 0.29.2 |
| `bazel build //src/... //tests/...` | All 23 targets built |
| `bazel test //src/... //tests/... --local_test_jobs=1` | All 7 test targets passed |
| `nm` on release static/shared artifacts | The 10 expected `fl_*` definitions; subsequent header updates only clarify contracts, with declarations/values unchanged from pre-migration HEAD |

From `cpp-bindings/`:

| Command / check | Result |
| --- | --- |
| `bash tools/test_cargo.sh` | C++17 consumer and example compiled with strict warnings, linked to Cargo's shared library, and passed the DFM conformance harness |
| `bazel build //:fault_reporter //tests:consumer //tests:e2e //examples/cpp_reporter` | Passed using the external provider and Rust static library |
| `bazel test //tests:consumer //tests:e2e` | Both test targets passed |
| Public header compile with `-std=c++17 -Wall -Wextra -Werror -pedantic` | Passed |
| Public header compile with `-std=c++14` | Rejected with the intentional C++17 requirement diagnostic |
| `bash -n tools/test_cargo.sh tests/e2e.sh` | Passed |
| Buildifier check of all changed/new BUILD and MODULE files | Passed |
| `git diff --check` in both repositories | Passed |

The downstream tests exercise live-handle move assignment, failed factories
preserving their destination, numeric/text/UUID construction, catalog strings,
reporter use after context destruction, thread-local diagnostics on foreign
threads, and exclusive ownership transfer to a thread while another reporter is
used on the main thread. DFM queries verify C publication, C++ publication,
metadata, example failure, and recovery. The standalone C11 consumer also mutates
its input storage after creation to check Rust-side copying. Production Bazel
dependency inspection confirms no dependency on `dfm_lib`; DFM is test-only.

### Architectural follow-up (2026-10-28)

The source-linked applicability/gap assessment is in
`fault-lib/src/fault_ffi/architecture.md`. Follow-up checks passed:

- Provider `cargo test --locked -p fault_ffi` and the three focused Bazel targets
  `//src/fault_ffi:{tests,c_consumer,e2e}`; downstream Cargo/native and both Bazel
  tests. Each integration suite includes the actual shared-library export check. C consumers compile
  the frozen v1 layout/type/value/signature baseline; C++ tests also assert standard
  layout and trivial copying of the three boundary structs.
- The new Rust tests verify `FlContext`/`FlReporter: Send + Sync` and containment
  of a panic payload whose destructor panics with another panicking payload. The
  strengthened payload test was rerun with `cargo test --locked -p fault_ffi --lib`.
- Negative compiler probes changed a status value, field width, field order and
  function signature independently; all were rejected by the frozen baseline.
  The original header passed. Synthetic shared artifacts with a missing API symbol
  or an extra internal export were rejected before any IPC setup.
- Generated header regeneration/check with cbindgen 0.29.2; declaration/value
  comparison with pre-split HEAD after removing comments/whitespace; focused Clippy,
  Rust formatting, shell syntax, changed-file buildifier and whitespace checks.
- ELF inspection of Cargo release and Bazel fastbuild shared artifacts found ten
  externally visible exports, BIND_NOW, GNU_RELRO, non-executable GNU_STACK and build
  IDs. Neither has a SONAME. DT_NEEDED names libc, the loader and libgcc_s only.
  The native C++ test executable uses RUNPATH to its development build directory.

Worker teardown/initialization source inspection showed that threads may outlive
handle destruction or failed initialization. The header and READMEs now require
the shared library to remain loaded until process exit after use. This is a
corrected lifetime contract; an unload-safe shutdown operation is not implemented.
The binary-release, toolchain, deployment and safety evidence still needed for
S-Core shared delivery is listed explicitly in the provider assessment.

## Failures and their disposition

- The initial downstream Bazel build selected an older default Rust compiler and
  rejected the provider's let-chains. **Fixed** by explicitly selecting Rust 1.91.0
  in `cpp-bindings/MODULE.bazel`. Subsequent builds/tests passed.
- Workspace-wide strict Clippy fails on three pre-existing wildcard imports at
  `fault-lib/src/fault_lib/src/test_utils.rs:22`, exposed by `--all-features`.
  That file is unchanged; the focused ABI lint check passes.
- `bazel test //:format.check //:copyright.check` fails its Starlark formatting
  test on unchanged `src/common/BUILD`, `src/dfm_lib/BUILD`, and
  `tests/integration/BUILD`. Python/YAML formatting passes. The changed/new
  Starlark files pass a focused buildifier check.
- `copyright.check` is a binary, so `bazel test` only builds it. Running
  `bazel run //:copyright.check` reports ten existing header/template mismatches.
  Its dependency-supplied template expects the older Eclipse Foundation banner
  and years 2024/2025, whereas these files already use the repository's 2026
  OpenSOVD SPDX style. Existing notices were retained, and the original LICENSE
  and NOTICE were copied byte-for-byte into `cpp-bindings`.

## Remaining integration scope

- Use the paired local checkouts for now. Publish/merge the provider changes,
  then replace the local override with an immutable upstream revision and wire
  automatic upstream CI to it. No nonexistent upstream release pin was invented.
- Root Bazel overrides and settings must remain aligned with the provider;
  its inherited mutable `rules_boost` archive remains a reproducibility issue.
- Validation covers native Linux x86-64. Cross-compilation, other operating
  systems, installed SDK packaging, Miri/sanitizer runs, and formal MISRA
  C++:2023 compliance were not verified in this migration.
- The existing ABI's one-successful-initialization policy, no-flush semantics,
  process-lifetime shared-library requirement, per-handle serialization, and
  unrecoverable abort/invalid-pointer cases remain contractual limitations, not
  consequences of repository placement.
