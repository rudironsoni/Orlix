---
type: concept
tags:
  - build-system
  - bazel
  - migration
  - artifacts
  - worktrees
updated: 2026-09-01
summary: "Migrate Orlix to a Bazel-owned repository product graph while preserving Make, upstream build engines, proof ownership, and worktree isolation."
relates_to:
  - "[Adopt the Bazel product graph](../objects/epic/doing/adopt-bazel-product-graph.md)"
  - "[ADR 0033](../objects/architecture-decision/0033-use-bazel-as-the-repository-product-graph.md)"
  - "[ADR 0034](../objects/architecture-decision/0034-consume-signed-promoted-buildsets.md)"
  - "[ADR 0035](../objects/architecture-decision/0035-isolate-worktree-build-state-and-share-content-caches.md)"
  - "[ADR 0036](../objects/architecture-decision/0036-use-generated-local-and-committed-cloud-xcode-projects.md)"
  - "[ADR 0037](../objects/architecture-decision/0037-stage-bazel-preparation-before-one-authority-cutover.md)"
  - "[ADR 0038](../objects/architecture-decision/0038-support-ios-and-ipados-15.md)"
---

# Bazel Product Graph Migration

## Outcome

Bazel becomes the repository-level product graph, cross-component dependency graph, test-selection graph, packaging graph, and artifact identity authority. Make remains the only supported repository-owned developer and CI interface.

Linux Kbuild, mlibc Meson and Ninja, and upstream Autotools and Make remain authoritative for dependencies inside their upstream components. Bazel invokes those engines directly with declared inputs and outputs. Bazel must not invoke the top-level Orlix Makefile or component wrapper Makefiles from a Bazel action.

The final authority change is one cutover. Preparatory rules, providers, shadow targets, manifests, and non-required parity checks can merge before that cutover when they do not compete with the current build authority.

## Invariants

The migration preserves these product rules:

1. Upstream Linux owns Linux semantics.
2. Durable Linux port inputs remain in the existing overlay, driver, configuration, and patch roots.
3. Generated Linux, mlibc, package, Xcode, and Bazel trees are read-only results.
4. OrlixHostAdapter owns private Apple and Darwin mechanics only.
5. OrlixMLibC consumes Linux UAPI only from upstream `headers_install` for `ARCH=arm64`.
6. Guest packages consume the OrlixMLibC sysroot and installed Linux UAPI contract.
7. OrlixOS owns distribution policy, rootfs assembly, payload metadata, environments, and the app-facing session API.
8. Orlix remains the terminal host and user interface.
9. `OrlixKernel.xcframework` remains a private product kernel artifact.
10. `OrlixOS.xcframework` remains the sole public SDK artifact.
11. Orlix profiles remain exactly `release` and `development`.
12. The two profiles remain userspace ABI invariant.
13. OrlixTCTI remains under `arch/orlix` and keeps the no-JIT, no-RWX, no-`MAP_JIT`, no-QEMU, no-Wasm, and no-host-executable-guest-text constraints.
14. KUnit, kselftest, OrlixMLibC tests, Linux-native syscall and package tests, and XCTest retain their current ownership.
15. A later proof tier never replaces an earlier proof tier.
16. A cached result is an optimization. It is not proof.
17. Promotion and release consume the exact tested artifacts. They do not rebuild those artifacts.
18. The app and public SDK support iOS and iPadOS 15.0. The optional Live Activity extension has an independent 16.1 minimum.

## Supported Command Surface

Humans and workflows call fixed Make targets such as:

```text
make build PROFILE=release
make build PROFILE=development
make test
make runtime-tests
make beta-archive
```

The current Make target inventory is authoritative until an accepted decision approves a change. Existing targets such as `xcodeproj`, `runtime-tests`, `orlix-tcti-kernel-tests`, `orlix-tcti-isa-host-tests`, and `orlix-tcti-isa-audit` must not disappear silently.

Direct `bazel query`, `bazel aquery`, and Build Event Protocol inspection are maintainer diagnostics. They are not the supported product interface.

## Configuration Dimensions

Keep these dimensions independent:

```text
Apple compilation mode: dbg or opt
Orlix profile: development or release
Destination: iphonesimulator or iphoneos
Component mode: source or promoted
Signing mode: unsigned, development, or distribution
Proof tier: explicit target selection
```

Xcode Debug and Release do not select an Orlix profile. The build must support a debug Apple build with either Orlix profile.

## Narrow Cross-Component Contracts

Custom rules exchange narrow providers:

```text
OrlixPreparedLinuxInfo
OrlixLinuxArchiveInfo
OrlixInstalledUapiInfo
OrlixKernelAppleProductInfo
OrlixLibcSysrootInfo
OrlixPackageTreeInfo
OrlixRootfsInfo
OrlixProofSubjectInfo
OrlixProofReportInfo
```

OrlixMLibC accepts `OrlixInstalledUapiInfo`. It cannot receive the Linux archive or Apple product provider. Guest packages accept `OrlixLibcSysrootInfo`. Application targets consume OrlixOS, not private component providers.

The final HostAdapter composition edge remains [UNVERIFIED] until a bounded symbol and link inventory identifies Linux archive imports, HostAdapter exports, boot entry points, callbacks, resource lookup, archive ordering, and framework visibility.

## Source And Promoted Modes

Source mode is required for component-changing pull requests, component promotion, nightly reconstruction, toolchain changes, and reproducibility audits.

Promoted mode is the normal input mode for app-only integration, Xcode Cloud, TestFlight, and release candidates. It consumes one signed buildset that pins all private components, toolchain identity, and the proof index. It never resolves a mutable `latest` tag.

The committed `artifacts.lock.json` selects the signed buildset and its component OCI digests. A workflow proposes lock changes through a normal pull request. No promotion workflow writes directly to `main`.

## Durable And Disposable Storage

Use these storage roles:

| Data | Storage | Authority |
| --- | --- | --- |
| Private promoted components | GHCR OCI artifacts | OCI digest, signature, provenance |
| Signed compatible component set | GHCR buildset artifact | Buildset digest and signature |
| Public `OrlixOS.xcframework.zip` and official release evidence | Immutable GitHub Release | Asset hashes, signatures, provenance |
| Bazel action results | Local disk cache, later remote cache | Disposable speed input |
| Repository downloads | Digest-verified local and Actions caches | Disposable speed input |
| CI reports and logs | GitHub Actions artifacts | Disposable evidence transport |

GitHub Actions artifacts and caches are not canonical component storage. GHCR is not a Bazel action cache.

## Parallel Worktrees

Each worktree owns all mutable build state:

```text
ORLIX_BUILD_ROOT=<worktree>/Build
Bazel output base=<worktree>/Build/Bazel/output-base
Kbuild O=<worktree>/Build/OrlixKernel/...
DerivedData=<worktree>/Build/DerivedData/...
generated Xcode project=<worktree>/Orlix.xcodeproj
CCACHE_BASEDIR=<literal current worktree root>
```

Worktrees may share only content-addressed or dependency-checked caches:

```text
~/Library/Caches/Orlix/Bazel/disk-cache       30 GiB, 30 days, namespaced by Bazel and Xcode build
~/Library/Caches/Orlix/Bazel/repository-cache 10 GiB, 90 days
~/Library/Caches/Orlix/ccache                  20 GiB, 30 days
```

No worktree shares a Bazel output base, Bazel server, execution root, mutable Kbuild output, `Build/`, DerivedData, or generated Xcode project.

Every action-cache namespace includes the exact Bazel version and Xcode build. This prevents input-discovery results from one Apple toolchain from entering another toolchain build. Promotion disables Bazel action-result reuse, ccache, persistent Kbuild output, and DerivedData. Digest-verified repository downloads and exactly verified tool installations remain allowed.

## Apple Toolchain And Xcode Projects

The Apple toolchain is externally installed, exactly selected, and completely identified. Identity includes Xcode version and build number, real `DEVELOPER_DIR`, Apple Clang, linker, Swift, device and simulator SDK versions and builds, Metal toolchain, deployment target, host macOS, and host architecture.

Local development uses a `rules_xcodeproj` generated project. Xcode Cloud uses a committed minimal `OrlixCloud.xcodeproj` with shared schemes, signing metadata, and one fixed Make delegation. The committed project must not duplicate the product source or dependency graph.

## Feasibility Stop Gate

Broad migration work stops unless the pinned stable Bazel and Apple rule matrix works with Xcode 26.6 without a permanent rules fork. The gate proves:

```text
Bazel 9.2.0
rules_apple 4.5.3
rules_swift 3.6.1
rules_xcodeproj 4.1.0
rules_swift_package_manager 1.21.0
all current Swift packages, including MLXSwift
Ghostty, Zig, Metal, libssh2, OpenSSL, and resource bundles
iOS 15.0 compilation for the app and public SDK
the reviewed MLXSwift compatibility fork at the iOS 15.0 package floor
the iOS 16.1 live activity extension as an optional embedded product
iOS 15.5 runtime installation through pinned `xcodes` in GitHub CI
the app launch smoke test on the created iOS 15.5 simulator
simulator build and launch
iphoneos compilation
archive and signing
indexing, navigation, breakpoints, previews, and test selection
one Kernel Kbuild archive and installed-UAPI output
mlibc built from that installed UAPI
the committed Xcode Cloud project shape
```

The repository must reject a selected Xcode other than the pinned version before it accepts a cache entry or promoted artifact.

The local host does not define the oldest supported runtime proof boundary.
GitHub CI installs iOS 15.5 with the verified `xcodes` CLI, creates a dedicated
simulator, and runs the Make-owned launch test. A local missing runtime cannot
remove, defer, or weaken iOS 15 compatibility work.

## Verification Classes

Use the correct test type:

| Test type | Scope |
| --- | --- |
| Bazel analysis test | Labels, attributes, providers, visibility, dependency direction, action declarations |
| Execution test | File content, digests, ABI equality, manifests, absolute-path rejection |
| Workflow policy test | Cache flags, action pins, permissions, tag and environment restrictions |
| Fault-injection test | Network attempts, stale inputs, tampered OCI data, wrong signatures, wrong profiles |

Do not assign execution-output checks to analysis tests.

## Proof And Promotion

Proof reports bind the tested subject digest, buildset digest, profile, destination, toolchain identity, prerequisite report digests, proof owner, result, logs, and forbidden behavior fields.

Product runtime proof keeps the accepted order:

```text
Kernel dependency proof
KUnit kernel-internal proof
kselftest kernel-interface proof
OrlixMLibC proof
OrlixMLibC-built syscall and UAPI proof
POSIX shell proof
jq proof
curl proof
zsh proof
product integration proof
```

Component promotion uses two independent clean source builds with unique output roots and disabled action-result caches. It compares reproducible unsigned outputs, runs owning proof, creates an SBOM and in-toto provenance, signs the OCI digest, publishes it, pulls it again, and verifies it again.

TestFlight and App Store promotion use the exact tested IPA. Release assembly does not rebuild private components or the application.

## GitHub Security

Protect `main`, release tag patterns, lock files, Bazel rules, toolchains, and workflows. Release tags cannot be updated or deleted, and the tagged commit must be reachable from protected `main`.

Private artifact signing uses a protected Cosign key unless a later accepted decision changes the trust model. The trust policy records accepted key IDs, validity windows, retirement, revocation, overlap, emergency replacement, allowed workflows, allowed refs, and allowed media types.

The private GitHub Pro repository must not rely on GitHub private-repository artifact attestations. Immutable Releases still protect official assets when enabled.

## Implementation Sequence

1. Create the migration work hierarchy and task envelope. Capture current targets, proof, outputs, timing, Xcode, and release behavior. Correct stale documentation.
2. Run the Xcode 26.6 feasibility stop gate.
3. Add Bzlmod bootstrap, independent settings, platforms, toolchain manifests, narrow providers, and policy tests.
4. Add verified upstream source rules and source mirrors.
5. Wrap Kbuild, installed UAPI, mlibc, Coreutils, packages, rootfs, initramfs, and ext4 without replacing upstream internal build definitions.
6. Add explicit HostAdapter, hosted-kernel composition, OrlixMachine, Containers, Herdr, OrlixOS, application, extension, and test targets.
7. Model the complete digest-bound proof graph.
8. Add component promotion, buildset promotion, artifact-lock proposal, provenance, signing, release, and garbage collection workflows.
9. Run old and Bazel graphs in shadow parity. Compare only outputs defined as reproducible.
10. Switch Make, required checks, release workflows, Xcode ownership, and artifact selection in one authority-cutover change.
11. Keep a read-only pre-Bazel rollback branch for one shipped release cycle and one clean reconstruction exercise.

## Cutover Acceptance

Cutover requires both profiles and both destinations, profile ABI invariance, every current proof tier, no generated-tree mutation, no undeclared network or host tools, cache and clean-build equivalence, two independent reproducible builds, exact TestFlight-to-App-Store promotion, Xcode developer behavior, active branch and tag rulesets, tamper tests, retention controls, rollback evidence, and a release manifest containing every component and proof digest.

Performance claims require measured clean, warm, edit, branch-switch, remote-cache, project-generation, indexing, and release scenarios. Record wall time, critical path, action counts, hit ratios, bytes, memory, CI minutes, storage growth, failure rate, and reproducibility mismatches.
