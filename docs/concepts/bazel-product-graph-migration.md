---
type: concept
tags:
  - build-system
  - bazel
  - migration
  - artifacts
  - worktrees
updated: 2026-09-11
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
7. OrlixOS is the running hosted Linux operating system. The internal OrlixDistribution concept owns guest distribution policy, rootfs assembly, payload metadata, environments, and the resources packaged by OrlixKit.
8. Orlix remains the terminal host and user interface. Its local runtime entry point is OrlixKit.
9. `OrlixKernel.xcframework` remains a private product kernel artifact.
10. `OrlixKit.xcframework` is the public embeddable SDK artifact. OrlixOS is the running OS, not the distribution artifact.
11. Orlix profiles remain exactly `release` and `development`.
12. The two profiles remain userspace ABI invariant.
13. OrlixTCTI remains under `arch/orlix` and keeps the no-JIT, no-RWX, no-`MAP_JIT`, no-QEMU, no-Wasm, and no-host-executable-guest-text constraints.
14. KUnit, kselftest, OrlixMLibC tests, Linux-native syscall and package tests, and XCTest retain their current ownership.
15. A later proof tier never replaces an earlier proof tier.
16. A cached result is an optimization. It is not proof.
17. Promotion and release consume the exact tested artifacts. They do not rebuild those artifacts.
18. The app and public SDK support iOS and iPadOS 15.0. The optional Live Activity extension has an independent 16.1 minimum.
19. OrlixEngine, OrlixBootloader, OrlixHostAdapter, and Kernel Mach-O integration are Apple-native implementation layers. OrlixMLibC, OrlixCoreUtils, guest packages, and rootfs/images are Linux guest or distribution artifacts and are not Apple-native link dependencies.

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

## Recovery identities and invalidation proof

The recovery keeps four identities separate:

```text
action identity
    every effective input that can change an action result
incremental-directory identity
    compatible mutable upstream state retained per worktree
artifact/content identity
    real output paths, bytes, modes, symlinks, and image content
proof/provenance identity
    tested subject, buildset, toolchain, policy, and proof evidence
```

Downstream actions consume only semantic product artifacts. Provenance, manifests, proof records, and source identities are not compilation inputs unless their contents semantically affect compilation. Consumers select specific provider fields instead of inheriting a producer's complete `DefaultInfo` output set.

### Guest artifact identity format

`artifact-identity-v2` identifies selected product content. Its canonical
serializer is [content_digest.py](../../bazel/content_digest.py). The manifest
contains the domain `orlix.artifact.identity`, version `2`, format
`artifact-identity-v2`, and entries sorted by relative path. Each entry records
its type and exact POSIX permission bits, including special bits. Regular
files record SHA-256 of their bytes. Symlinks record their target without
following it. Tree selection includes empty directories. The containing
directory's host path is not part of the product identity.

The manifest uses ASCII-escaped JSON, sorted keys, compact separators, and one
final newline. Its SHA-256 is the artifact digest. Consumers MUST retain the
format, hash algorithm, and digest together. The content namespace is
`artifact-identity-v2/sha256/<digest>`; an unqualified digest cannot select a
format. Equivalent selected entries have the same identity regardless of
their build directory or selection method.

Product selection MUST exclude proof and provenance through declared artifact
boundaries. The serializer does not ignore files by name. A real product file
named `provenance.json` is still product content. Timestamps do not affect the
identity, but path, type, permission, file-content, and symlink-target changes
do. Callers MUST hash the delivered artifact representation and MUST NOT erase
mode differences to make a comparison pass.

Existing signed digests retain their original serializer and schema. They
MUST NOT be relabelled, overwritten, or treated as `artifact-identity-v2`.
Promotion of a new format requires independent source reproduction, its own
proof, a signature, and a reviewed lock proposal. Unknown formats fail before
reuse or promotion. Unsigned Phase 5 records cannot change
`artifacts.lock.json`.

Phase 6 verification MUST bind this content identity to the OCI manifest
digest, signing identity, trust policy, and verification-policy version.
These verification facts remain separate from action identity and the
worktree-local incremental-directory identity. A verification-policy change
requires verification again; it does not by itself change product content.

The mutation proof MUST inspect both Bazel action execution and actual upstream compiler or build-engine execution. The required cases are:

| Mutation | Required work | Work that MUST remain reusable |
| --- | --- | --- |
| Orlix.app Swift/UI leaf | Affected app target and downstream app link/package | Engine and all Linux foundations |
| vvterm-derived terminal surface | Affected terminal/app targets | Kernel, mlibc, Coreutils, packages, rootfs |
| OrlixEngine implementation | Engine and downstream Kit/app products | Kernel, mlibc, Coreutils, packages, rootfs |
| OrlixBootloader implementation | Affected Bootloader objects and downstream links | Linux/Kbuild, mlibc, Coreutils, packages, rootfs |
| OrlixHostAdapter implementation | Affected HostAdapter objects and downstream links | mlibc, Coreutils, packages, rootfs; Kernel unless an interface changes |
| Internal Kernel implementation with unchanged UAPI | Affected Kbuild objects and Kernel products | mlibc, Coreutils, packages, rootfs unless they embed changed Kernel resources |
| Installed Linux UAPI change | Installed headers and affected mlibc/packages/rootfs | Unrelated native host code |
| OrlixMLibC implementation | Affected mlibc objects and true downstream consumers | Kernel, Bootloader, HostAdapter, unchanged compiler-rt |
| OrlixCoreUtils implementation | Affected Coreutils outputs and rootfs/resources | Kernel, mlibc compilation, Bootloader, HostAdapter |
| Rootfs policy-only change | Rootfs assembly and downstream resource packaging | Kernel, mlibc, Coreutils compilation |

The proof also covers TCTI edits, removed inputs, tool or configuration changes, interrupted builds, corrupted cache entries, concurrent worktrees, and garbage collection during active use. A provenance-only change may update evidence or packaging, but MUST NOT recompile unchanged semantic consumers.

## Developer-loop benchmark contract

The benchmark MUST measure these sixteen scenarios with the same declared toolchain and workload before and after the relevant optimization checkpoint:

1. clean clone to build;
2. warm no-op build;
3. Orlix.app Swift leaf edit;
4. vvterm terminal edit;
5. OrlixEngine edit;
6. OrlixBootloader edit;
7. OrlixHostAdapter edit;
8. internal Kernel implementation edit;
9. TCTI implementation edit;
10. OrlixMLibC implementation edit;
11. OrlixCoreUtils edit;
12. rootfs-only policy edit;
13. second worktree at the same commit;
14. branch or worktree switch;
15. cold promoted-artifact acquisition; and
16. warm promoted build with all locked bytes already local.

Each run records wall-clock duration, critical-path duration, executed actions, Bazel action-cache hits and misses, disk-cache hits and misses, remote-cache hits and misses, compiler-cache hits and misses, bytes downloaded, bytes written, peak memory, temporary disk amplification, and persistent disk growth. Warm cases run three times and report each run and the median. A warm promoted hit performs zero artifact downloads. The benchmark compares real result content, not marker files or cache-hit counts.

## Recovery checkpoints

The approved recovery order is:

1. Encode the OrlixKit, Engine, Bootloader, OS, Instance, Process, Container, and OrlixDistribution authority.
2. Remove unsafe persistent UAPI/output reuse.
3. Preserve Kernel prepared-source and Kbuild incremental state.
4. Preserve mlibc Meson/Ninja state and extract compiler-rt.
5. Preserve Coreutils and guest-package incremental boundaries.
6. Make promoted reconstruction local-first and digest-addressed.
7. Correct the OrlixKit, Engine, Bootloader, HostAdapter, and app dependency graph.
8. Improve app compilation granularity without moving terminal UI into OrlixKit.
9. Add invalidation and developer-loop benchmark proof.
10. Perform one final parity and build-authority cutover.

Each checkpoint MUST leave a diagnosable record in `IMPLEMENT.md`, including changed authority, exact checks and exit codes, artifact identities, evidence paths, and unresolved gates. Checkpoint records do not claim implementation or runtime completion until their owning proof exists.

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

OrlixMLibC accepts `OrlixInstalledUapiInfo`. It cannot receive the Linux archive or Apple product provider. Guest packages accept `OrlixLibcSysrootInfo`. Application targets consume OrlixKit, not private component providers or guest build providers. Consumers select specific semantic artifact fields rather than inheriting a producer's complete `DefaultInfo` output set. Provenance, source manifests, proof records, and source identities are not compilation inputs unless their contents semantically affect the result.

The final HostAdapter composition edge remains [UNVERIFIED] until a bounded symbol and link inventory identifies Linux archive imports, HostAdapter exports, boot entry points, callbacks, resource lookup, archive ordering, and framework visibility. This evidence cannot make guest artifacts Apple-native link dependencies.

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
| Public `OrlixKit.xcframework.zip` and official release evidence | Immutable GitHub Release | Asset hashes, signatures, provenance |
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

Every action-cache namespace includes the exact Bazel version and Xcode build. This prevents input-discovery results from one Apple toolchain from entering another toolchain build. The action identity also includes every effective input that can change the result, while an incremental-directory identity remains stable across source edits that an upstream engine can rebuild. Promotion disables Bazel action-result reuse, ccache, persistent Kbuild output, and DerivedData. Digest-verified repository downloads and exactly verified tool installations remain allowed.

The shared immutable promoted-artifact store is local-first. It stores OCI manifests, blobs, signatures, provenance, and verification records by digest. A warm hit for every locked component performs zero artifact downloads. Verification records bind the artifact digest, signing key, trust-policy identity, and verification-policy version. Locked buildsets and active builds are pinned before cache garbage collection; only unused, unpinned content may be evicted.

The cache split is explicit:

```text
Action/content cache        reusable declared results and immutable blobs
Incremental build state     mutable Kbuild, Meson/Ninja, Autotools, and Xcode state per worktree
Compiler cache              bounded content-addressed compiler objects shared across worktrees
Promoted products           signed immutable guest and native products addressed by digest
```

Deleting, disabling, or corrupting any cache may reduce speed only. It must not change a product result. Downstream compilation consumes semantic product artifacts, not provenance or proof metadata unless that metadata changes compilation semantics.

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
iOS 15.0 compilation for the app and public OrlixKit SDK
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
6. Add explicit HostAdapter, OrlixBootloader, OrlixEngine, hosted-kernel composition, OrlixInstance, OrlixContainer, Herdr, OrlixOS, OrlixKit, application, extension, and test targets. Guest distribution artifacts remain resource inputs and are not Apple-native implementation dependencies.
7. Model the complete digest-bound proof graph.
8. Add component promotion, buildset promotion, artifact-lock proposal, provenance, signing, release, and garbage collection workflows.
9. Run old and Bazel graphs in shadow parity. Compare only outputs defined as reproducible.
10. Switch Make, required checks, release workflows, Xcode ownership, and artifact selection in one authority-cutover change.
11. Keep a read-only pre-Bazel rollback branch for one shipped release cycle and one clean reconstruction exercise.

## Cutover Acceptance

Cutover requires both profiles and both destinations, profile ABI invariance, every current proof tier, no generated-tree mutation, no undeclared network or host tools, cache and clean-build equivalence, two independent reproducible builds, exact TestFlight-to-App-Store promotion, Xcode developer behavior, active branch and tag rulesets, tamper tests, retention controls, rollback evidence, and a release manifest containing every component and proof digest.

Performance claims require measured clean, warm, edit, branch-switch, remote-cache, project-generation, indexing, and release scenarios. Record wall time, critical path, action counts, hit ratios, bytes, memory, CI minutes, storage growth, failure rate, and reproducibility mismatches.
