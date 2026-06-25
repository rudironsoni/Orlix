# Goal

Deliver real OCI/container-image-derived Linux environments inside Orlix beside default Orlix Linux. This is feature delivery, not rejection coverage, parser theater, proof packages, stamp ladders, or host simulation. Target a capable Linux terminal on iOS with OCI roots, Linux process execution, PTYs, filesystems, namespaces, cgroups, networking, devices, `orlix run`, and registry input.

## Architecture

- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, Orlix arch port, Linux-native drivers, boot code, or narrow Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. It must not own Linux policy, ABI, syscalls, OCI lifecycle, packages, cgroups, namespaces, filesystems, shells, or runtime semantics.
- OrlixMLibC is userspace libc. Keep it unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux-surface issue, fix Linux.
- OrlixOS is the Kit/session layer. It owns OCI import, descriptors, rootfs assembly, payload metadata, environment selection, lifecycle records, app-facing APIs, and Linux setup orchestration.
- Do not recreate OrlixKit, revive retired local-kernel paths, invent Orlix ABIs, clone Linux UAPI, or expose Darwin-shaped public surfaces.
- Virtio is an internal device plane below Linux subsystems, not public ABI, VM lifecycle, runtime substitute, or Linux policy copy.

## Linux Surface

Implement cgroups, namespaces, devices, mounts, procfs, sysfs, devfs, devpts, sockets, signals, wait/reaping, fd tables, exec, interpreters, filesystem semantics, terminal behavior, and resource control through Linux-owned code and Linux-visible interfaces. OrlixOS may call Linux syscalls and write procfs/sysfs/cgroupfs/devfs config, but must not duplicate these subsystems in Swift, HostAdapter, fixtures, feature reports, or test metadata. Host adaptation stays private in HostAdapter or Orlix port layer and invisible above OrlixKernel.

## Build Efficiency

Linux, OrlixMLibC, and Coreutils build acceleration must use proven owning build-system mechanisms: Kbuild, Meson/Ninja, Autotools/Make, and optional `ccache`/`sccache`. Accelerators are caches only. Cache misses, absent tools, stale directories, or changed inputs fall back to the owning build system. Do not invent package stores, freshness databases, manifest skip engines, or proof metadata to decide builds are correct.

## Proof

Proof is the test suite: upstream Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils/upstream package tests, OrlixOS/XCTest runtime tests, hosted terminal proofs, and Linux-oracle comparisons. Package, rootfs, and fixture assembly may exist only to launch those suites. OCI feature reports may claim only implemented behavior backed by tests. Parser rejections, fixtures, stamps, package manifests, or feature JSON are not readiness proof.

## Product Model

One OrlixKernel runs multiple Linux environments: default Orlix root, imported rootfs environments, and OCI-derived environments. OCI metadata may configure an environment, but Linux semantics remain owned by Linux. Product behavior must feel like a real Linux terminal on iOS, not a parser demo, unsupported-field report, host compatibility layer, or custom runtime facade.

## Non-Actions

Do not add Docker daemon, `runc`, Apple Containerization runtime, Virtualization.framework, Linux VM lifecycle, `vminitd`, `vmnet`, Rosetta, custom OCI syscalls, HostAdapter-owned Linux semantics, Darwin/Foundation/POSIX host APIs in OrlixKernel, generated upstream edits, mlibc patches masking Linux defects, proof-package or stamp-ladder systems, feature-report-only implementations, or duplicated OrlixOS cgroup/namespace/device/filesystem/network subsystems.

## Goal Size

Keep `GOAL.md` under 4000 characters. Put design detail, proof logs, checkpoints, and tactical order in `PLAN.md` and `IMPLEMENT.md`.
