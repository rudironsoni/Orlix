---
type: architecture-decision
tags:
  - architecture
  - decision
  - bazel
  - caching
  - product-boundary
updated: 2026-09-10
status: accepted
external_id: "ADR-0040"
summary: "Make OrlixKit the public runtime boundary and recover correct incremental, immutable, and promoted build reuse."
part_of:
  - "[Orlix](../product/orlix.md)"
amends:
  - "[ADR 0004](0004-use-a-bootloader-only-product-api.md)"
  - "[ADR 0013](0013-package-real-linux-artifact-before-ios-execution.md)"
  - "[ADR 0018](0018-ground-kernel-proof-in-app-hosted-runtime.md)"
  - "[ADR 0020](0020-build-linux-as-mach-o-native-orlixkernel-framework.md)"
  - "[ADR 0021](0021-keep-libc-out-of-orlixkernel-boundaries.md)"
  - "[ADR 0022](0022-use-hosted-linux-elf-execution.md)"
  - "[ADR 0023](0023-use-release-development-profiles-and-curated-orlixos-distribution.md)"
  - "[ADR 0024](0024-adopt-orlix-native-application-foundation.md)"
  - "[ADR 0025](0025-make-herdr-authoritative-for-terminal-topology.md)"
  - "[ADR 0026](0026-use-one-kernel-with-namespaced-orlix-machines.md)"
  - "[ADR 0027](0027-use-app-store-only-cross-platform-host-integration.md)"
  - "[ADR 0028](0028-provide-full-docker-engine-compatibility-through-orlixos.md)"
  - "[ADR 0030](0030-use-one-public-orlixos-sdk-and-private-static-implementation-layers.md)"
  - "[ADR 0033](0033-use-bazel-as-the-repository-product-graph.md)"
  - "[ADR 0034](0034-consume-signed-promoted-buildsets.md)"
  - "[ADR 0035](0035-isolate-worktree-build-state-and-share-content-caches.md)"
  - "[ADR 0036](0036-use-generated-local-and-committed-cloud-xcode-projects.md)"
  - "[ADR 0037](0037-stage-bazel-preparation-before-one-authority-cutover.md)"
  - "[ADR 0038](0038-support-ios-and-ipados-15.md)"
relates_to:
  - "[Bazel product graph migration](../../concepts/bazel-product-graph-migration.md)"
  - "[Lifecycle and namespaces](../../concepts/lifecycle-and-namespaces.md)"
  - "[Component ownership](../../concepts/component-ownership.md)"
---

# ADR 0040: Recover OrlixKit Product Boundaries And Build Reuse

## Status

Accepted for PR #228 recovery. This decision records the target architecture and build contracts. It does not claim that the current source graph, runtime, proof ladder, or release process already satisfies them.

The key words **MUST**, **MUST NOT**, **REQUIRED**, **SHOULD**, **SHOULD NOT**, and **MAY** in this document are to be interpreted as described in RFC 2119.

## Authority precedence

For this recovery, the product boundary and terminology in this decision supersede conflicting existing SDK and lifecycle naming clauses. The implementation agent MUST encode this decision into repository authority before using the updated documents to govern later phases. Unrelated clauses of the amended ADRs remain authoritative.

The recovery MUST proceed through coherent, independently verified checkpoints on the existing branch. A large final pull request is not a reason to couple unrelated changes or to promote a partial result. Each checkpoint records its changed authority, verification, evidence, and unresolved gates in the repository's implementation checkpoint record.

## Product vocabulary and topology

The canonical product vocabulary is:

```text
Orlix
    First-party iOS application and product.

OrlixKit
    Public embeddable Swift SDK and XCFramework.
    Packaging and API boundary, not a runtime object.

OrlixEngine
    Process-wide iOS-hosted execution engine.
    Owns host lifecycle and hosts at most one OrlixOS.

OrlixBootloader
    Private native boot mechanism.

OrlixOS
    The running hosted Linux operating system.
    It uses one upstream Linux kernel.

OrlixInstance
    A persistent isolated Linux userspace hosted by OrlixOS.

OrlixProcess
    An ordinary Linux process.

OrlixContainer
    An OCI container belonging to one OrlixInstance.
```

The runtime topology is:

```text
iOS application
    -> OrlixKit.xcframework
        -> OrlixEngine
            -> one OrlixOS
                -> one upstream Linux kernel
                -> zero or more OrlixInstances
                    -> OrlixProcesses and OrlixContainers
```

`OrlixDistribution` is an internal build and packaging concept. It names the guest resources assembled for OrlixOS:

```text
OrlixDistribution
    -> OrlixMLibC-built userspace
    -> OrlixCoreUtils
    -> guest packages
    -> rootfs and filesystem images
```

`OrlixOS` MUST mean the running OS in the runtime API. The build graph MUST NOT equate OrlixOS with a rootfs or with the distribution inputs from which its guest userspace is assembled.

`Orlix.app` remains the complete first-party application. Its vvterm-derived terminal surface, Ghostty integration, remote connections, SSH, Mosh, files, settings, navigation, CloudKit, telemetry, and developer UX remain in the app. The app MUST enter its local Linux runtime through OrlixKit and MUST NOT directly depend on OrlixBootloader, OrlixKernel, OrlixHostAdapter, OrlixMLibC, OrlixCoreUtils, TCTI implementation targets, or Kernel Mach-O composition targets.

A third-party application MUST be able to use OrlixKit to launch an OrlixProcess through pipes without constructing a terminal or linking first-party terminal code.

## Ownership and dependency boundaries

OrlixKit contains two dependency classes that remain distinct:

```text
Apple-native implementation
    OrlixEngine
    OrlixBootloader
    OrlixHostAdapter
    OrlixKernel Mach-O integration

Linux guest and distribution resources
    OrlixMLibC
    OrlixCoreUtils
    guest packages
    rootfs and filesystem images
```

OrlixMLibC, OrlixCoreUtils, guest packages, and rootfs MUST remain Linux guest or distribution artifacts. They MUST NOT become Apple-native link dependencies, including through an aggregate target that hides their providers. OrlixKit MAY package or reference the resulting resource artifacts.

Upstream Linux owns Linux behavior, including VFS, tasks, file descriptors, signals, wait and reaping, procfs, sysfs, devtmpfs, cgroups, namespaces, sockets, syscall semantics, `execve`, and interpreters. OrlixKernel remains the upstream Linux port and Orlix architecture or driver implementation. OrlixHostAdapter owns private Apple and Darwin mechanics only. OrlixEngine owns process-wide host lifecycle and boot orchestration and MUST NOT become a Darwin-side Linux policy facade.

OrlixMLibC consumes installed Linux UAPI through `headers_install`. OrlixCoreUtils remains ordinary Linux userspace linked against OrlixMLibC. Linux policy and syscall behavior do not move into Swift, OrlixEngine, OrlixKit, or OrlixHostAdapter.

## Build graph, identities, and cache contracts

Bazel owns the repository-level product, cross-component dependency, Apple product, test-selection, packaging, and artifact identity graphs after the final authority cutover. Make remains the supported repository-facing command interface. Kbuild, Meson/Ninja, Autotools, and other upstream build engines retain internal dependency ownership.

Downstream build actions MUST consume only the semantic product artifacts that can affect their outputs. Provenance, source manifests, proof records, and source identities MUST NOT be inputs to downstream compilation unless their contents semantically affect that compilation. Consumers MUST select specific provider fields rather than inherit every producer output.

The following identities are distinct:

1. **Action identity** includes every effective source, generated input, configuration, dependency artifact, tool, flag, platform, and build-rule input that can change the action result.
2. **Incremental-directory identity** selects compatible mutable Kbuild, Meson/Ninja, Autotools, DerivedData, or other upstream state. It MUST remain stable across ordinary source edits that the upstream engine can rebuild incrementally.
3. **Artifact/content identity** describes the actual output bytes and metadata, including paths, contents, modes, symlink targets, and canonical image content where applicable.
4. **Proof/provenance identity** binds evidence to the tested artifact, buildset, toolchain, inputs, policy, and proof tier. It MUST NOT silently become a compile dependency.

Deleting, disabling, corrupting, or missing any cache MUST only make the next build slower. It MUST NOT change the produced result. Persistent state is an accelerator, never authority. A cache miss MUST invoke the owning upstream build engine with reconciled declared inputs. Corrupt or incomplete incremental state MUST be rebuilt.

Kernel source preparation, installed UAPI, Kernel products, compiler-rt, mlibc, each guest package, rootfs assembly, native implementation, and OrlixKit packaging MUST expose narrow artifact boundaries. A Kernel implementation change that leaves installed UAPI byte-identical MUST permit mlibc and unaffected userspace artifacts to remain reusable. A genuine UAPI change MUST fan out to affected consumers.

Mutable build state MUST remain worktree-local. Bazel content, repository downloads, compiler objects, prepared immutable content, and promoted artifacts MAY be shared only through bounded content-addressed or dependency-checked storage. Active builds and the locked promoted buildset MUST be pinned against garbage collection.

Promotion uses two independent clean source builds. The reproducibility comparison disables Bazel action-result reuse, compiler cache, persistent Kbuild/Meson/Autotools state, and DerivedData. Promoted artifacts are immutable, digest-addressed, signed, and verified. A warm promoted build with all locked bytes present locally MUST perform zero artifact downloads. Missing bytes require acquisition and verification; a cache hit alone never authorizes promotion or release.

## Engine boot compatibility

`OrlixEngine.boot(configuration:)` uses one immutable, versioned process-wide boot identity. The identity MUST include every setting that can alter Kernel or OS boot, including:

- the Orlix profile where it changes Kernel implementation or diagnostics;
- Kernel configuration identity;
- Kernel command-line identity, including order and duplicate arguments where meaningful;
- Kernel product identity;
- process-wide boot-resource identities;
- HostAdapter ABI and boot-interface identity; and
- other process-wide Kernel inputs.

Instance-specific rootfs, hostname, namespaces, cgroups, processes, containers, and terminal configuration MUST NOT enter the Engine boot identity.

Concurrent compatible boot requests MUST converge on one boot operation and one OrlixOS handle. A later compatible request MUST return that handle. An incompatible request MUST fail deterministically, including while boot is in progress, and MUST NOT reboot, replace, or mutate the running OS. Opening, closing, or resizing a terminal MUST NOT change the Engine boot identity.

## Instance, process, and OCI lifecycle

OrlixOS MAY host zero or more persistent OrlixInstances. Each instance owns Linux init, filesystem state, PID, mount, UTS, IPC, network, and user namespaces, plus a cgroup v2 subtree. Instances share one Kernel and MUST NOT imply separate Kernels or VMs. Stopping preserves state. Deletion affects only the selected instance.

The migration MUST preserve existing user data and adopt the current default writable state into the default instance through restart-safe metadata changes. Process-wide bootstrap MUST work with zero instances and without terminal geometry. New instances start stopped; the default instance starts on demand.

OrlixKit MUST expose process launch with arguments, environment, working directory, and pipes or PTY I/O. Pipes provide distinct binary stdin, stdout, stderr, EOF, and exit status. PTY mode adds terminal geometry and resize. Process lifecycle comes from Linux execution and wait events, not boot-log parsing or caller-supplied PIDs.

The framed transport MUST identify requests, instances, processes, and streams, retain bounded frames, and apply backpressure. Boot diagnostics remain separate from process streams.

OrlixContainer MUST belong to one OrlixInstance. The recovery implements OCI `create`, `start`, `state`, `kill`, `delete`, process exec, and wait with normal Linux lifecycle semantics. Failed creation cleans up acquired resources. Docker and Compose compatibility remain separate conformance targets.

Runtime proof MUST establish isolation of process and PID state, mounts and root filesystem state, UTS and hostname state, network namespace identity and namespace-local state, user and credential namespace state, IPC state, and cgroups. Functional external networking MUST be claimed only to the proof tier established by the current virtio and network implementation.

## Proof and rollout gates

The recovery is implemented as independently verified checkpoints. Checkpoint 1 records this authority. Later checkpoints cover unsafe persistence, Kernel incrementalism, mlibc and compiler-rt, package boundaries, promoted reuse, Kit and runtime lifecycle, app graph granularity, invalidation and benchmark proof, parity, and one final authority cutover.

Physical-device TCTI and product validation MUST NOT run until all of the following establish current passing simulator-ladder evidence and `physical_device_allowed=true`:

```text
make agent-status AREA=orlix-tcti
make agent-next AREA=orlix-tcti
make agent-task-envelope-check AREA=orlix-tcti
```

Simulator, golden-ELF, first-syscall, or partial-package evidence MUST NOT be promoted to physical-device proof. The Coreutils TAP lane remains stopped until explicitly authorized. Its required success marker is:

```text
ORLIX-COREUTILS-TEST-END failures=0 skips=0
```

Parity, cache equivalence, invalidation locality, developer-loop benchmarks, Apple matrix gates, runtime proof, and protected automation MUST pass before cutover. The final report MUST state what was changed, what each identity contains, what is shared or local, which mutations rebuilt, cache hit behavior, cold and warm timings, disk changes, passed proof tiers, and every unproved requirement.

## Rejected shortcuts

Renaming OrlixOS or OrlixMachine without implementing the boundary, hiding guest providers behind an Apple-native aggregate, treating a Bazel cache hit as proof, using provenance as a broad compile input, sharing mutable worktree state, repeatedly downloading warm immutable artifacts, using terminal logs as process proof, or using weaker TCTI evidence as device proof does not satisfy this decision.
