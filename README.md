# Orlix

Orlix is the first-party iOS application and product. The product goal is to run Linux userspace inside an iOS app through the public `OrlixKit.xcframework`. OrlixKit exposes `OrlixEngine`, which boots and hosts one running `OrlixOS`. OrlixOS uses one upstream Linux kernel and hosts persistent `OrlixInstance` userspaces.

The SDK and lifecycle migration is in progress. [ADR 0040](docs/objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md) defines the required architecture, and [IMPLEMENT.md](IMPLEMENT.md) records verified checkpoints. Current public Swift sources remain under `OrlixOS/Sources/Session`; the new component names below describe the accepted target ownership.

Orlix.app is the complete first-party experience. It keeps its terminal, vvterm-derived surface, Ghostty integration, remote connections, files, settings, navigation, CloudKit, and telemetry. It enters the local Linux runtime through OrlixKit and does not link private Kernel, Bootloader, HostAdapter, libc, Coreutils, TCTI, or Mach-O composition targets directly. Linux owns Linux after boot.

OrlixKernel is Linux. It does not provide a shell, libc, package manager, public syscall API, or fake runtime facade. Shells and packages are normal Orlix Linux userspace binaries linked against OrlixMLibC and executed through Linux mechanisms.

## ELI5 Start

Source and component ownership:

- `Build/OrlixKernel/upstream/linux-<version>.git` is the generated bare upstream Linux clone. Treat it as read-only input.
- `OrlixKernel/Sources/ports/orlix` is where durable Orlix Linux port inputs live.
- `OrlixMLibC/Sources` is the durable component area for OrlixMLibC sysdeps, configs, and patches. The upstream mlibc bare clone is generated under `Build/OrlixMLibC/upstream/mlibc-<version>.git`, and the patched working source is generated under `Build/OrlixMLibC/src/mlibc-<version>`.
- `OrlixKit` is the public embeddable Swift SDK and XCFramework. It packages the private Apple-native implementation and the guest distribution resources needed by OrlixOS.
- `OrlixOS` is the running hosted Linux operating system. Its internal `OrlixDistribution` resources contain OrlixMLibC-built userspace, OrlixCoreUtils, guest packages, and rootfs/images.
- `OrlixEngine` owns process-wide host lifecycle and hosts at most one running OrlixOS. `OrlixInstance` owns persistent isolated Linux userspace state inside that OS.
- `OrlixHostAdapter/Sources` is where private iOS and Darwin mechanics live.

Project source and test roots are organized consistently:

```text
OrlixKernel/Sources
OrlixKernel/Tests
OrlixHostAdapter/Sources
OrlixHostAdapter/Tests
OrlixMLibC/Sources
OrlixMLibC/Tests
OrlixOS/Sources
OrlixOS/Tests
Orlix/Sources
Orlix/Tests
```

Each project has its own `Makefile`; the top-level `Makefile` orchestrates calls into those project Makefiles.

The legacy local-kernel prototype has been retired from the tracked source tree. It is not the target architecture and is not product proof. Do not restore `LegacyOrlix/`, `OrlixKernel/fs`, `OrlixKernel/kernel`, or `OrlixKernel/runtime`. Useful behavior belongs by ownership in upstream Linux-native paths.

## Proof Model

Proof is claim-promoted, not flat. Work may happen in parallel, but product claims must follow ADR 0017:

1. Kernel dependency proof
2. Kselftest kernel-interface proof
3. OrlixMLibC libc proof
4. OrlixMLibC-built syscall/UAPI proof
5. POSIX shell environment proof
6. Third-party package ladder: jq, curl, zsh

Orlix does not require `vmlinux` as a canonical build, proof, or runtime artifact. The canonical OrlixKernel proof artifact is the iOS app-hosted OrlixKernel integration that actually runs inside the Orlix app environment. A `vmlinux`-style artifact may exist only as an optional developer/debug artifact with a named consumer. It is not a milestone, not product proof, not runtime proof, not libc proof, and not required for installed UAPI headers.

KUnit proves kernel-internal behavior. OrlixMLibC-built kselftests prove Linux kernel-interface and libc-to-kernel syscall/UAPI behavior. mlibc tests prove OrlixMLibC. Bash proves the first interactive POSIX shell environment. jq, curl, and zsh prove increasingly realistic third-party package compatibility.

Do not claim product runtime readiness from KUnit, kselftest, boot logs, packaging, or a host-side harness.

## Build And Test Commands

The top-level Makefile keeps a small, Linux-shaped interface and delegates to `OrlixKernel/Makefile`, `OrlixHostAdapter/Makefile`, `OrlixMLibC/Makefile`, `OrlixOS/Makefile`, and `Orlix/Makefile`:

```bash
make help
make setup-env
make build
make test
make test type=kunit,kselftest
make clean
```

In the current pre-cutover source path, `make setup-env` fetches upstream Linux as a bare clone and generates the disposable Xcode project from `project.yml`. `make build` preserves `Build/` and delegates the selected product build to the component Makefiles. Use `make rebuild` only when you intentionally need `make clean` before `make build`. This path materializes upstream mlibc as a bare clone plus patched working source under `Build/OrlixMLibC`, builds the guest sysroot from durable inputs, and stages guest distribution assembly under `Build/OrlixOS`. It does not prove terminal runtime behavior or require `vmlinux` as a normal artifact.

The current Kernel lane emits per-profile, per-platform static archives under `Build/OrlixKernel/<profile>/<platform>/OrlixKernel.a`. Xcode packages matching slices as the private `OrlixKernel.xcframework`. The recovery moves public packaging to `OrlixKit.xcframework`, which MUST package or reference OrlixDistribution guest resources, including rootfs/images. Resource names remain declared in target metadata. OrlixOS denotes the running hosted OS.

`PROFILE=release` is the default profile. Pass another profile only when you intentionally need it.

Use variables for scope instead of target-name variants: `PROFILE=...`, `type=...`, and `libc=...`. Proof labels are emitted inside artifacts and logs; they are not public Make targets.

The Linux-shaped lower-level targets are available when needed:

```bash
make prepare PROFILE=release
make headers_install PROFILE=release
make kunit PROFILE=release
make kselftest PROFILE=release
make kselftest PROFILE=release libc=orlixmlibc
```

`make prepare` materializes the generated upstream-plus-Orlix port tree and Kbuild output without requiring a standalone kernel image. `make headers_install` installs Linux UAPI headers into `Build/OrlixMLibC/kernel-headers/<profile>/include` and does not consume `vmlinux`.

`make kunit` currently builds Linux KUnit-selected Orlix test objects. That is useful dependency evidence, not iOS-hosted KUnit execution proof. KUnit proves kernel-internal behavior only after the hosted Linux proof path runs it and emits Linux-owned KUnit output.

`make kselftest` uses OrlixMLibC-built kselftests under `Build/OrlixMLibC/kselftest/<profile>/`. That lane requires an OrlixMLibC sysroot plus installed Orlix UAPI headers and is the syscall/UAPI proof lane.

Do not run kselftest or KUnit on Darwin and do not use a VM as product proof. Do not add repo-local shell or standalone C contract tests as milestone proof. Linux kernel-internal behavior belongs in KUnit. Linux kernel-interface behavior belongs in selected kselftests. Orlix userspace ABI and product runtime claims require the later ADR 0017 proof lanes.

Both `iphoneos` and `iphonesimulator` are iOS proof destinations. Milestones must validate the same scope on both.

XCTest suites are organized under project-local test trees. `OrlixKernel/Tests/XCTest/OrlixKernelHostedTests` launches the lower-level bootloader path, `OrlixOS/Tests/XCTest` covers OrlixOS payload/session wiring, and `OrlixHostAdapter/Tests/XCTest/OrlixHostAdapterTests` covers narrow host mechanics. Linux KUnit and kselftest results remain authoritative and are consumed through the app-hosted kernel conformance path rather than reinterpreted by a separate XCTest fixture parser.

Milestone 5 boot-to-virtio-probe proof keeps the dependency chain honest. Static DTS, defconfig, and kselftest source inputs are preparatory only. The milestone is proved only when iOS-hosted Orlix Linux consumes the profile device tree and reaches the point where upstream virtio-mmio probing can be attempted.

Milestone 5 does not prove `/dev/vda`, `/dev/vdb`, virtio-block request I/O, host-backed disk persistence, initramfs loading, OverlayFS root assembly, or general userspace boot.

## Generated Trees

The pristine upstream Linux bare clone is generated at:

```text
Build/OrlixKernel/upstream/linux-<version>.git
```

The disposable upstream-plus-Orlix port tree is generated at:

```text
Build/OrlixKernel/src/linux-<version>-port
```

The upstream mlibc bare clone and patched source tree used by OrlixMLibC builds are generated at:

```text
Build/OrlixMLibC/upstream/mlibc-<version>.git
Build/OrlixMLibC/src/mlibc-<version>
```

If a kernel change should survive regeneration, put it in `OrlixKernel/Sources/ports/orlix`, not in a generated tree. If an mlibc change should survive regeneration, put it in durable OrlixMLibC inputs such as `OrlixMLibC/Sources/patches`, not in the generated upstream mlibc source tree.

## Port Inputs

Durable Orlix Linux port inputs live under:

```text
OrlixKernel/Sources/ports/orlix/
  overlay/
  patches/
  configs/
```

`overlay` contains files copied into Linux-native paths such as `arch/orlix` and `drivers/orlix`.

`patches` contains minimal upstream-tree deltas that cannot be represented as overlay files.

`configs` contains product profile defconfigs such as `release_defconfig` and `development_defconfig`.

## Product Surface

The public app-facing product surface is `OrlixKit`. It exposes OrlixEngine, OrlixOS, OrlixInstance, OrlixProcess, and OrlixContainer handles. A third-party app may launch an OrlixProcess through pipes without constructing a terminal. OrlixKit must not expose syscall, file, mount, task, cgroup, package-manager, or private host-mechanics APIs.

OrlixMLibC, OrlixCoreUtils, guest packages, and rootfs/images are Linux guest or distribution artifacts. They may be packaged by OrlixKit, but they are not Apple-native private link dependencies.

## Current Proof Boundary Snapshot

This section is status context, not durable architecture truth. Refresh it from the active plan and latest evidence before using it to scope or claim work.

The current blocking proof boundary is iOS-hosted kernel-interface execution. The branch is source-layout and build-hook aligned, not runtime aligned. Real-artifact XCFramework packaging is a prerequisite, not product runtime proof.

Success at this boundary means the iOS host launches packaged OrlixKernel and collects dependency proof from the running kernel path, such as KUnit output, Linux-accurate no-init behavior, or selected OrlixMLibC-built kselftests. It does not mean Bash, jq, curl, zsh, or product runtime compatibility is proved.

## Device Direction

Orlix is virtio-first where Linux already has upstream device classes.

Use upstream Linux behavior for Linux-visible devices:

- `virtio_blk` for root disks
- `virtio_console` for the main console path
- `virtio-rng` for entropy
- `virtio_net` for networking
- virtio-fs first, or 9p over virtio if needed, for external directory mounts

Orlix-specific code supplies transport and backend mechanics under `drivers/orlix`, shaped as close to Linux virtio conventions as possible.

## Read Deeper

The canonical architecture specification is:

```text
docs/concepts/upstream-linux-ownership.md
```

Architecture decisions are recorded under:

```text
docs/objects/architecture-decision/
```

Glossary terms resolved during design live in:

```text
docs/concepts/domain-language.md
```
