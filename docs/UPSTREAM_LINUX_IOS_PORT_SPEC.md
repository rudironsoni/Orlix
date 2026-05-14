# Upstream Linux iOS Port Architecture Spec

This document defines the approved architecture for the Orlix upstream Linux iOS port skeleton.

## Core Position

Orlix compiles upstream Linux for iOS-hosted products. The local repository owns the Orlix port overlay, patch set, configs, bootloader-facing product API, private host mediation, and build/package proof targets.

The upstream Linux source tree is generated local input. It is not the durable place for Orlix changes.

## Upstream Source Rule

`make bootstrap-linux-upstream` materializes upstream Linux into:

```text
Linux/upstream/linux-6.12
```

That directory is ignored and generated. Treat it as read-only input.

Do not commit generated upstream Linux files. Do not make durable Orlix changes directly in the generated upstream tree.

## Disposable Worktree Rule

`make prepare-linux-worktree` produces:

```text
Build/linux-work
```

That worktree is disposable. It is assembled from:

- `Linux/upstream/linux-6.12`
- `Linux/ports/orlix/overlay`
- `Linux/ports/orlix/patches`

Any change made only in `Build/linux-work` is temporary. Durable changes must be moved into the committed Orlix port overlay, patch set, configs, or product API source.

## Local Rewrite Rule

Orlix must not locally rewrite Linux core subsystems as a fake runtime facade.

The local port may add architecture glue, Orlix Linux drivers, boot preparation, build integration, and minimal port patches. It must not replace upstream Linux core behavior with local substitutes for scheduler, VFS, signals, syscalls, process model, device model, memory management, procfs, sysfs, devfs, cgroups, or userspace execution.

If the skeleton cannot prove a Linux runtime capability, do not document or expose it as complete.

## Port Overlay Ownership

Committed Orlix Linux port files live in:

```text
Linux/ports/orlix/overlay
```

The overlay is copied into Linux-native paths in the disposable worktree.

### Architecture Glue

`arch/orlix` owns Orlix architecture glue inside the prepared Linux tree.

In the repository, durable files for that tree live under:

```text
Linux/ports/orlix/overlay/arch/orlix
```

This area may define Orlix architecture Kconfig, Makefiles, architecture setup, architecture headers, and architecture-owned boot preparation.

### Orlix Linux Drivers

`drivers/orlix` owns Orlix Linux drivers inside the prepared Linux tree.

In the repository, durable files for that tree live under:

```text
Linux/ports/orlix/overlay/drivers/orlix
```

This area may define Linux drivers needed by the Orlix port. Driver code must remain Linux driver code, not a public host facade.

### Boot Preparation

`boot/` under the Orlix architecture tree owns boot preparation for the architecture port.

In the repository, durable files for that area live under:

```text
Linux/ports/orlix/overlay/arch/orlix/boot
```

Boot preparation may prepare the bootloader-facing handoff state. The skeleton must not claim real entry into `start_kernel` until that is proven by the build and runtime flow.

## Patch Ownership

Committed patch files live in:

```text
Linux/ports/orlix/patches
```

Use patches only for intentional upstream-tree changes that cannot be represented as overlay files. Keep patches narrow, reviewable, and tied to upstream Linux integration needs.

Do not use patches to create local rewrites of Linux core subsystems.

## Config Ownership

Committed config inputs live in:

```text
Linux/ports/orlix/configs
```

Configs define build variants for the Orlix Linux port. They do not prove runtime capability by themselves.

## Host Adapter Boundary

`OrlixHostAdapter/` owns private iOS/Darwin mechanics only.

Allowed responsibilities include private host mediation needed by the Orlix port, such as host file access, packaging support, or platform-specific mechanics behind narrow Orlix-owned seams.

Forbidden responsibilities include:

- defining Linux policy
- exporting Linux syscall or libc APIs
- becoming the public runtime ABI
- replacing upstream Linux core behavior
- exposing Darwin semantics as Linux semantics

Host mechanics stay private. Linux behavior belongs to upstream Linux plus explicit Orlix Linux port code.

## Public Product API

The bootloader-facing product surface is declared in:

```text
OrlixKernel/include/OrlixKernel.h
```

The public API is limited to:

- `struct boot_params`
- `OrlixPrepareBootParams`
- `OrlixBoot`

Do not add public syscall, filesystem, driver, libc, process, or runtime facade APIs to this header in the skeleton.

## Build Proof Targets

The Makefile proof targets for this skeleton are:

```bash
make build-linux-simulator
make build-linux-iphoneos
make package-orlixkernel-xcframework
```

These targets prove that the upstream source can be materialized, the Orlix port overlay and patches can prepare a disposable worktree, and the simulator/device slices can be packaged as an XCFramework.

They do not prove real boot, real syscall entry, real Linux drivers, rootfs mount, bundled userspace, procfs/sysfs/devfs/cgroupfs runtime behavior, or App Store execution policy.

## Documentation Rule

Documentation must describe the skeleton honestly.

Allowed claims:

- upstream Linux source is generated local input
- Orlix durable port code lives in overlay, patches, and configs
- `Build/linux-work` is disposable
- `arch/orlix` owns architecture glue
- `drivers/orlix` owns Orlix Linux drivers
- architecture `boot/` owns boot preparation
- `OrlixHostAdapter/` owns private iOS/Darwin mechanics
- the public API is bootloader-facing and intentionally small

Forbidden claims without proof:

- real boot to `start_kernel`
- real syscall entry
- complete Linux driver behavior
- mounted rootfs
- bundled Linux userspace
- procfs/sysfs/devfs/cgroupfs runtime proof
- App Store execution-policy support
- local replacement of Linux core subsystems as an Orlix runtime facade
