# Context Glossary

## Local Kernel Prototype

The current `OrlixKernel/fs`, `OrlixKernel/kernel`, and `OrlixKernel/runtime` implementation tree. It is not part of the target architecture. Its useful behavior must be migrated into upstream Linux-native extension points under `arch/orlix`, `Linux/ports/orlix/overlay`, and `drivers/orlix`; after migration, these directories should no longer exist.

## No-New-Local-Kernel-Work Rule

No new Linux subsystem behavior should be added to `OrlixKernel/fs`, `OrlixKernel/kernel`, or `OrlixKernel/runtime`. Migration may read from these directories for behavior reference, but target work belongs in upstream Linux, `arch/orlix`, Linux-native drivers, boot, or host-adapter seams.

## Test Migration Rule

Tests for the local kernel prototype are migration reference, not authoritative proof for the target architecture. New proof should be KUnit for kernel-internal Linux behavior, kselftest for Linux user-visible behavior, and XCTest for iOS-hosted launch, packaging, Linux test-output collection, and narrow `OrlixHostAdapter` host mechanics.

## Linux-Native Test Proof

KUnit and kselftest are the tests of record for Linux-owned behavior. Building those artifacts is only preparatory evidence; runtime proof requires executing them inside Orlix Linux and collecting KUnit KTAP plus kselftest TAP from that Linux instance.

## XCTest Proof Harness

The iOS-side proof harness that packages or launches `OrlixKernel.xcframework`, starts Orlix through the bootloader-shaped API, collects Linux test output from inside Orlix Linux, and verifies host-mechanics behavior without replacing KUnit or kselftest assertions.

## Linux Test Output Collection

A private host-side collection path for Linux-native test output. KUnit output is collected from the Linux kernel log path, kselftest output is collected from test-initramfs stdout, and XCTest parses those streams without becoming a public test-management API.

## Separate Linux Test Streams

The proof rule that KUnit and kselftest raw outputs remain separate streams. XCTest parses each independently and reports a combined verdict only after both pass.

## Test Initramfs Sequence

The test initramfs first collects KUnit output from the kernel log path and KUnit debugfs when enabled, then runs `run_kselftest.sh -c orlix` and captures kselftest stdout separately.

## KUnit DebugFS Affordance

A test-kernel option set that enables Linux debugfs plus KUnit debugfs and exposes per-suite KUnit KTAP under `/sys/kernel/debug/kunit/<suite>/results`. It is useful to the test initramfs and XCTest proof path, but it is not public `OrlixKernel` API.

## OrlixTerminal

The iOS terminal app that hosts `OrlixKernel.xcframework`, launches Orlix through the bootloader-shaped API, presents the terminal surface, and serves as the XCTest host for iOS-hosted proof.

## Terminal UI Surface

The libghostty-spm-provided terminal presentation layer used by `OrlixTerminal`. It renders terminal I/O but does not own Linux execution or shell semantics.

## Orlix Terminal Backend

The Orlix-owned terminal byte path between Linux console/terminal plumbing and the terminal UI surface. It replaces sandbox shell execution backends such as `ShellCraftKit`.

## No-Fake-Terminal Rule

`OrlixTerminal` must not use fake shells, sandbox shells, or local execution backends to simulate Linux terminal behavior. Before Linux console bytes exist, it may display only real Orlix boot or proof output.

## iOS-Targeted Build

An Xcode build for both `iphoneos` and `iphonesimulator` that packages or launches an `ARCH=orlix` Linux artifact through the iOS host surface. Both destinations are iOS proof destinations and must validate the same milestone scope; `iphoneos` targets physical devices, `iphonesimulator` targets Simulator, and neither means compiling Linux as an Apple iOS ABI.

## XcodeGen Project Manifest

The committed `project.yml` file that defines Orlix's iOS packaging and XCTest proof topology. It is durable source; the generated `.xcodeproj` is disposable output unless a future toolchain constraint requires otherwise.

## iOS Proof Parity

The requirement that the same XCTest suite and assertions pass on both `iphoneos` and `iphonesimulator`, with destination-specific wiring allowed only for mechanics such as signing, resource lookup, transport, or host-adapter details.

## Simulator-First Implementation

A development order that brings up the iOS harness on `iphonesimulator` first for speed. It does not reduce the milestone proof requirement; completion still requires the same proof on both `iphoneos` and `iphonesimulator`.

## OrlixKernel XCFramework Slice Set

The initial `OrlixKernel.xcframework` platform slices are `ios-arm64` for physical iOS devices and `ios-arm64-simulator` for Apple Silicon Simulator. Intel Simulator support is not part of the initial slice set.

## Profile Linux Artifact Parity

For a selected Orlix profile, `iphoneos` and `iphonesimulator` slices wrap the same bit-identical `ARCH=orlix` Linux artifact. Destination differences belong in the iOS host wrapper or `OrlixHostAdapter`, not in the Linux kernel artifact.

## Selected Profile Framework Build

An `OrlixKernel.xcframework` build that packages exactly one selected profile's `ARCH=orlix` Linux artifact while also bundling the closed built-in profile DTBs.

## OrlixKernel Wrapper

The iOS Mach-O framework or static-library surface inside each `OrlixKernel.xcframework` slice. It exposes the bootloader-shaped public API while bundling or loading the `ARCH=orlix` Linux artifact and private boot resources as payload data.

## Linux Payload Artifact

The real `ARCH=orlix` Linux build output, initially `vmlinux`, packaged as private payload data inside or alongside the iOS wrapper. It is proof input and boot payload, not the iOS linkable framework binary.

## Local-Kernel XCTest Reference

The existing XCTest coverage for `OrlixKernel/fs`, `OrlixKernel/kernel`, and `OrlixKernel/runtime`. It is migration reference only; Linux subsystem assertions should move to KUnit or kselftest, while retained XCTest should cover iOS-hosted Orlix launch, Linux test-output collection, packaging, or narrow host mechanics.

## iOS-Hosted Linux Test Execution

The proof path where an iOS host app or XCTest target launches `OrlixKernel.xcframework`, boots Orlix Linux with test resources, and captures KUnit KTAP plus kselftest TAP from inside the Linux instance. Darwin-hosted execution, QEMU or VM execution, and repo-local shell harnesses are not substitutes.

## Ownership-Based Migration

The process for moving from the local kernel prototype to the target architecture. Existing behavior is kept only when it still belongs under the upstream-Linux iOS port model; behavior that upstream Linux owns is deleted rather than reimplemented locally.

## Linux Surface Rule

When upstream Linux has a user-visible surface, implementation convention, build/test flow, or ownership model for a problem, Orlix follows that Linux shape instead of inventing an Orlix-specific equivalent unless a concrete iOS constraint forces a documented exception.

## Orlix Kernel Port Tree

The disposable upstream-Linux source tree after applying the Orlix port overlay and patch set. Its path is `Build/OrlixKernel/linux-<version>-port`. Durable changes must move back to the committed port overlay, patch set, configs, or bootloader-facing product surface.

## Real Linux Build Proof

Evidence that upstream Linux is being built as Linux for `ARCH=orlix`, with `vmlinux` as the first honest proof artifact. `OrlixKernel.xcframework` packaging follows early as the iOS execution enabler, but must package a real Linux artifact rather than substitute boot stubs for Linux proof.

## XCFramework Packaging Milestone

The third milestone for the upstream-Linux iOS port. It packages a real Orlix Linux artifact into `OrlixKernel.xcframework` before iOS-hosted Linux test execution; boot-stub packaging is not proof.

## App Store Profile

The default Orlix build profile. Normal build targets use this profile unless explicitly overridden, because it carries the strictest product constraints.

## Development Profile

An Orlix build profile that should match the App Store profile except for explicit debug and testing affordances. It must not become a noisier or broader product shape.

## Profile Parity

The rule that App Store and development profile differences are limited to explicit debug and testing affordances. All product behavior, Linux-visible device shape, boot resource roles, and milestone proof scope should otherwise remain equal.

## Test Build Overlay

A test-only kernel configuration layer applied to both App Store and development proof builds to enable KUnit, kselftest support, KUnit debugfs, and related proof affordances without changing the normal product profile configs.

## Orlix KUnit Config

A committed `.kunitconfig` under the Orlix architecture overlay that selects Orlix KUnit suites and their KUnit-specific dependencies, matching upstream KUnit practice.

## KUnit Proof Merge

The proof-build step that merges the selected profile defconfig with `arch/orlix/.kunitconfig` for both App Store and development proof kernels. Normal product builds use only the selected profile defconfig.

## Profile Proof Parity

The requirement that milestones claiming iOS packaging, boot, runtime, or Linux behavior validate both App Store and development profiles against the same XCTest scope.

## iOS Proof Matrix

The required four-cell XCTest proof matrix for milestones claiming iOS packaging, boot, runtime, or Linux behavior: App Store on `iphoneos`, App Store on `iphonesimulator`, development on `iphoneos`, and development on `iphonesimulator`.

## Proof Matrix Orchestrator

The repository `make` target that runs the iOS proof matrix non-interactively. XcodeGen defines the Xcode topology and schemes, but `make` owns repeatable proof orchestration.

## Milestone Proof Target

An explicit repository target used to claim milestone completion. Heavy proof such as the four-cell iOS proof matrix belongs here, not in a generic fast local check.

## Boot Profile

A closed product-profile choice exposed through the bootloader entrypoint. Supported profiles are App Store and development; arbitrary string-named profiles are not part of the public API.

## Profile Defconfig

A durable Orlix product-profile configuration stored under `Linux/ports/orlix/configs/`. During port-tree generation, the selected profile is materialized into Kbuild's expected architecture config location for the generated tree.

## Bootloader Entrypoint

The public way the host app starts Orlix. It is minimal and represents booting Linux, not calling a runtime management API. The public API receives a small app-level boot config; the bootloader derives Linux-shaped boot inputs from profile device trees and command-line defaults.

## Boot Config

The minimal app-level input to the bootloader entrypoint. The first public shape contains only a boot profile, a root image identifier, and a terminal identifier.

## Resource Identifier

An opaque app-level name for a host-backed boot resource. The bootloader resolves resource identifiers through `OrlixHostAdapter`; raw iOS paths and host handles are not Linux-visible truth.

## Root Device

The Linux-visible default root storage devices for Orlix. `/dev/vda` is the immutable bundled base image and `/dev/vdb` is the writable app-private state image; the mounted root is assembled above them with upstream Linux mechanisms.

## Root Filesystem

The main Linux filesystem for Orlix. It is assembled from virtio-blk-backed Linux filesystem images using upstream Linux mechanisms; external directory mechanisms such as virtio-fs or 9p are separate explicit mounts, not the root filesystem.

## App Store Root Storage

The App Store root storage model uses an immutable bundled base image plus writable app-private state or overlay storage. Persistent Linux state belongs in app-private storage, while caches remain recreatable and external documents are explicit mounts.

## Package State

Linux package databases and permitted installed package state live under normal Linux paths in writable state. Pre-bundled packages live in the immutable base image, while repositories, post-install behavior, and downloaded content are constrained by profile policy.

## App Store Package Channel

The App Store profile may allow downloaded binary packages only through curated, signed, profile-approved repositories with App Store-safe disclosure and policy checks. It is not an unrestricted arbitrary Linux repository model.

## Package Policy Ownership

Repository trust, package signatures, metadata, and post-install policy are userspace package-policy responsibilities guided by the selected profile. Kernel and architecture code enforce hard execution constraints; `OrlixHostAdapter` does not become a package manager.

## Executable Memory Policy

The App Store profile follows normal Linux execution controls for file-backed executable mappings, including filesystem permissions, mount flags, memory-management behavior, and upstream security mechanisms. Unavoidable iOS host constraints such as writable-plus-executable denial are adapted through the architecture/MM boundary.

## Executable Content Trust

Executable content follows normal Linux package-manager and filesystem trust. Tools such as apt/dpkg verify packages and install files into the filesystem; after installation, execution is governed by normal filesystem permissions, mount flags, Linux security policy, and architecture/MM constraints.

## Execution Policy Rule

Orlix does not introduce a custom execution policy layer unless a concrete App Store or iOS host constraint cannot be represented with normal Linux package, mount, permission, memory-management, or upstream security mechanisms.

## Host Memory Adaptation

Unavoidable iOS memory mechanics are reached through narrow `arch/orlix`-owned seams into `OrlixHostAdapter`. Virtio is used for virtual devices, not for Linux MM or executable-memory decisions.

## Architecture Host Seams

Non-device host mechanics such as clocks, timers, execution substrate, low-level memory mapping, lifecycle notification, and very-early entropy may use narrow `arch/orlix`-owned seams into `OrlixHostAdapter`. Device-like runtime services should use virtio where possible.

## Lifecycle Ownership

App lifecycle handling is split between `arch/orlix` for unavoidable suspend/resume and timekeeping consequences, and standard Linux-visible power-management or event mechanisms for userspace observability when needed. A custom lifecycle character device is not the default target.

## Lifecycle Semantics

iOS backgrounding should map to Linux suspend/resume where feasible. App termination without an explicit saved image means the Linux instance ended and the next launch is a new boot with persistent filesystems intact; a future saved image should use Linux hibernation/resume semantics.

## Hibernation Scope

Hibernation/resume is deferred beyond the first architecture milestone. Early lifecycle plumbing should avoid blocking a future Linux-shaped hibernation path, but the first milestone only needs fresh boot plus suspend/resume hooks.

## Milestone Planning

The upstream-Linux iOS port should be planned as a sequence of focused milestones rather than one large migration. Each milestone must produce honest Linux-shaped proof before the next layer depends on it.

## Kbuild vmlinux Proof Milestone

The first milestone for the upstream-Linux iOS port. It is limited to source-tree generation, profile selection, Kbuild `vmlinux` proof for `ARCH=orlix`, and architecture documentation/instruction alignment. Required proof includes `make prepare-orlixkernel-port PROFILE=appstore`, `make build-linux-kernel PROFILE=appstore`, and `make build-linux-kernel PROFILE=development`. It does not implement virtio, boot API redesign, root filesystem assembly, or console behavior.

## Architecture Documentation Alignment

Milestone 1 must align `README.md`, `AGENTS.md`, and the canonical upstream-Linux iOS port specification with the target architecture. Documentation must stop presenting skeleton packaging or the local kernel prototype as the product direction.

## Canonical Spec Rewrite

`docs/UPSTREAM_LINUX_IOS_PORT_SPEC.md` should be created fresh from the resolved upstream-Linux architecture decisions rather than patched from stale architecture text.

## Canonical Spec Scope

The canonical architecture spec should be medium-detail: enough to prevent wrong architecture work, but not an exhaustive implementation manual. It should focus on rules, ownership, proof, and milestones.

## ADR Scope

Architecture Decision Records should capture durable architecture decisions from the upstream-Linux port design, not minor wording or command confirmations. ADRs are appropriate for choices that are hard to reverse, surprising without context, and based on real trade-offs.

## ADR Timing

Architecture Decision Records for the upstream-Linux port should be created during Milestone 1 documentation work after the shared design decisions are stable, not one-by-one during discovery.

## AGENTS Role

`AGENTS.md` is the concise strict rule set for agents working in the new upstream-Linux architecture. Broader rationale, milestones, and narrative belong in the canonical architecture specification.

## Agent Test Rules Cleanup

Old detailed `OrlixKernelTests` and KUnit/XCTest migration rules should be removed from `AGENTS.md` during the architecture rewrite. The retained rule is that local-kernel tests are migration reference, not proof for the target upstream-Linux architecture.

## README Role

`README.md` is the product overview and beginner-friendly working guide for the repository. It should explain what Orlix is and provide an ELI5 path for starting work without replacing the full architecture specification.

## README Flow

The README should explain concepts before commands: what Orlix is, what the repo owns, the directories contributors must know, the first commands to run, current milestone success criteria, and where to read deeper.

## Build Target Compatibility

Old build target names such as `prepare-linux-worktree` are not preserved as compatibility aliases. Build targets should be renamed to match the upstream-Linux OrlixKernel architecture directly.

## XCFramework Packaging Rule

`OrlixKernel.xcframework` packaging is required before iOS-hosted Linux execution proof can advance and must depend on a real Linux build artifact for the selected profile. Boot-stub packaging must not masquerade as product proof.

## Boot-Stub Packaging

Packaging `OrlixKernel.xcframework` from boot stubs alone is not a valid product target and should be removed in the first milestone. Narrow bootloader tests may remain only if they do not claim product packaging proof.

## Boot Entrypoint Proof

Proof that the public boot entrypoint remains bootloader-shaped and derives Linux-shaped boot input from closed profiles and resource identifiers. It must migrate to KUnit, kselftest, and XCTest rather than repo-local shell scripts or standalone C contract binaries.

## Boot Entrypoint Milestone

The second milestone for the upstream-Linux iOS port. It introduces the minimal bootloader entrypoint, closed boot profile selection, profile device trees, and Linux-shaped boot input generation while avoiding raw boot parameters as the product API.

## iOS-Hosted Linux Test Execution Milestone

The fourth milestone for the upstream-Linux iOS port. It launches packaged Orlix Linux from an iOS host app or test host, runs Linux-native KUnit and kselftest inside Orlix Linux, and captures KUnit KTAP plus kselftest TAP as proof. It does not prove virtio-block devices, root assembly, or interactive userspace.

## Boot To Virtio Probe Milestone

The fifth milestone for the upstream-Linux iOS port. It carries boot beyond prepared inputs so Linux can consume profile device tree data and reach the point where upstream virtio-mmio probing can be attempted. It depends on the iOS-hosted Linux test execution path for runtime proof and does not prove virtio-block device creation, block I/O, root assembly, or userspace boot.

## Virtio Root Disk Milestone

The sixth milestone for the upstream-Linux iOS port. It introduces Orlix's virtio-mmio-shaped transport under `drivers/orlix`, binds upstream `virtio_blk`, and exposes `/dev/vda` and `/dev/vdb` as the immutable base and writable state disks through `OrlixHostAdapter` backing.

## Root Assembly Milestone

The seventh milestone for the upstream-Linux iOS port. It loads the bundled immutable initramfs, mounts virtio-blk-backed base and writable state disks, assembles the root with upstream OverlayFS, and preserves Linux-shaped writable state paths.

## Console Milestone

The eighth milestone for the upstream-Linux iOS port. It provides minimal early console diagnostics, serial-style console support, upstream virtio-console selection, and host terminal byte I/O needed for early interactive boot.

## Virtio Devices Milestone

The ninth milestone for the upstream-Linux iOS port. It adds remaining virtio-first devices such as virtio-rng, virtio-net, and external directory mounts through virtio-fs or 9p where feasible. It may be split if the scope becomes too large.

## Root Overlay

The App Store root filesystem is assembled with upstream Linux OverlayFS when supported by the lower and upper filesystems. Initramfs mounts the immutable base image and writable state image, then switches to the merged root.

## Writable State Layout

The persistent writable root state mirrors normal Linux paths, especially `/etc`, `/var/lib`, `/home`, and package database locations. OverlayFS technical directories are early-boot implementation details, not the conceptual storage model.

## Cache Storage

Recreatable Linux cache data is separated from persistent writable state. The App Store profile should expose cache-backed storage as a distinct Linux-visible mount, such as `/var/cache`, so host cache loss cannot corrupt persistent identity.

## Temporary Storage

The default `/tmp` storage for Orlix is upstream Linux `tmpfs`. Host temporary directories are not Linux-visible truth unless a specific backend need is later justified.

## Initramfs Policy

Orlix supports normal Linux initramfs/initrd behavior. The App Store profile defaults to initramfs for early policy and root setup, but direct `root=/dev/vda` boot remains a supported Linux-shaped path.

## App Store Initramfs

The App Store profile uses an external initramfs artifact that is bundled with the app, signed as app content, immutable at runtime, and loaded by the bootloader through normal Linux boot data.

## Virtio-Block Semantics

The root block path must honor the Linux virtio-block contract rather than only using virtio-style device names. Host-backed storage is a mechanism behind the virtio/block implementation, not the Linux-visible interface.

## Virtio-Block Ownership

Upstream Linux `virtio` and `virtio_blk` own the Linux-visible root disk behavior. Orlix-specific code supplies the transport and host-backed backend needed to make that upstream driver work inside the iOS app boundary.

## Virtio-First Devices

Orlix should use upstream Linux virtio device classes wherever they fit, including block, console, entropy, and networking. Orlix-specific code supplies virtio transport and backend mechanics; upstream Linux drivers own Linux-visible behavior.

## Network Device Ownership

Linux-visible networking should use upstream Linux networking and upstream `virtio_net` where host-backed network access is needed. Loopback is upstream Linux loopback, and Orlix-specific code stays behind transport, backend, and policy seams.

## Custom Orlix Network Driver

A non-virtio Orlix network device driver. It is not part of the current target layout and should only be introduced if upstream Linux plus `virtio_net` cannot satisfy a concrete requirement.

## External Directory Mounts

Linux-visible mounts for user-selected or document-backed host directories. These should use upstream Linux filesystem mechanisms first, preferably virtio-fs and otherwise 9p over virtio; custom Orlix filesystems are not the first target.

## Orlix Virtio Transport

The Orlix-specific virtio transport lives under `drivers/orlix`, while its internal structure and contracts should stay as close as possible to Linux virtio transport conventions. Upstream virtio device drivers remain the Linux-visible owners of block, network, console, and entropy behavior when applicable.

## Virtio-MMIO Shape

The first Orlix virtio transport model is virtio-mmio shaped. Profile device trees should describe normal virtio-mmio-style devices where practical, while Orlix-specific code under `drivers/orlix` supplies the backend mechanics.

## Console Device

The Linux-visible boot console for Orlix. Orlix should support both a serial-style console and a virtio-console path, with normal Linux boot-time console selection deciding which console or consoles are active.

## Console Selection

The Linux-shaped process of selecting active boot consoles through kernel boot arguments and registered console drivers. Orlix should allow choosing between serial-style console behavior and virtio-console behavior at boot, matching regular Linux expectations.

## App Store Console Profile

The default console policy for the App Store profile. Both virtio-console and serial-style console support are enabled; virtio-console is the normal interactive path, and the serial-style console remains available for early, debug, or fallback use.

## Early Console

A minimal `arch/orlix` diagnostic path used before normal Linux console drivers are ready. It must hand off to registered Linux console drivers and must not become the main terminal implementation.

## Console Parity

The staged path for making the Orlix console Linux-correct. The driver should first register a real Linux console/TTY device, then add bidirectional byte I/O, termios basics, blocking and nonblocking behavior, window-size propagation, and later PTY/session/job-control integration.

## Boot Template

A profile-selected set of Linux-shaped boot artifacts, especially device tree data and kernel command-line defaults. It is not a custom Orlix file format; the bootloader uses it to produce normal Linux boot inputs.

## Profile Device Tree

The static Linux-shaped device tree source for an Orlix profile. Durable profile device trees live under the Orlix architecture overlay, for example `Linux/ports/orlix/overlay/arch/orlix/boot/dts/appstore.dts`, and the bootloader supplies dynamic boot-time values.

## Bundled Profile DTB

The compiled device tree blob for a closed built-in Orlix profile, shipped inside `OrlixKernel.xcframework` as part of the kernel port machine description. App Store constraints require built-in profile DTBs to be bundled rather than arbitrary host-supplied files.

## Test Initramfs Resource

A Linux-shaped test payload bundled with the XCTest host app, not with the product `OrlixKernel.xcframework`. It contains the minimal userspace needed to run kselftest and emit TAP without becoming part of the product framework contract.

## kselftest Install Shape

The upstream kselftest build/install flow used to stage Orlix kselftest binaries for the test initramfs. Manual copying from the build tree is only a temporary fallback when upstream install flow is blocked.

## kselftest Proof Runner

The installed `run_kselftest.sh` script used inside the test initramfs to execute Orlix kselftests. Direct binary execution is debugging only, not milestone proof.

## Orlix kselftest Collection

The installed kselftest collection selected with `run_kselftest.sh -c orlix` inside the test initramfs, even when only `TARGETS=orlix` is installed.

## Orlix kselftest Target Scope

The initial kselftest proof scope is `TARGETS=orlix`. Other upstream kselftest targets are added intentionally as the relevant Linux subsystems become available.

## kselftest Timeout Policy

The upstream timeout model where selftests default to 45 seconds per test. Orlix adds a test-local `settings` file only when a concrete test needs a non-default timeout; milestone fatality is decided by the XCTest proof runner, not hidden inside the test binary.

## kselftest Timeout Override

An explicit `run_kselftest.sh --override-timeout` value supplied by the XCTest proof runner. It is not used initially and should be added only for a concrete iOS proof-runner need.

## Product Initramfs

The later product/root-assembly boot payload for normal Orlix startup. It is separate from the test initramfs and is not designed by the early iOS-hosted test-execution milestone.
