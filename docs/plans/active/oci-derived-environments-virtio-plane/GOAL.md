# Goal

Deliver real OCI/container-image-derived Linux environments inside Orlix beside the default Orlix Linux root. Outcome: a capable Linux terminal on iOS that can run OCI-derived roots through Linux execution, PTYs or inherited stdio, filesystems, namespaces, cgroups, networking, devices, `orlix run`, registry input, and lifecycle control.

This is feature delivery. Do not spend effort on rejection coverage, parser theater, proof packages, stamp ladders, host simulation, or unsupported reports.

## Architecture

- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, the Orlix arch port, Linux-native drivers, boot code, or narrow Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. It must not own Linux policy, ABI, syscalls, packages, cgroups, namespaces, filesystems, or shells.
- OrlixMLibC consumes the Linux surface. Keep it unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux issue, fix Linux.
- OrlixOS is the Kit/session layer. It owns OCI import, descriptors, rootfs assembly, payload metadata, environment selection, lifecycle records, app-facing APIs, and Linux setup orchestration through Linux-visible interfaces.
- Virtio is an internal device plane below Linux subsystems, not public ABI, VM lifecycle, runtime substitute, or Linux policy copy.

Do not recreate OrlixKit, revive retired local-kernel paths, invent Orlix ABIs, clone Linux UAPI, expose Darwin public surfaces, or add HostAdapter Linux semantics.

## Linux Surface

Implement cgroups, namespaces, devices, mounts, procfs, sysfs, devtmpfs, devpts, sockets, signals, wait/reaping, fd tables, exec, interpreters, filesystems, terminals, resources, networking, and lifecycle through Linux-owned code.

No duplicated Linux subsystems in OrlixOS or HostAdapter. OrlixOS may prepare inputs and call Linux syscalls or procfs/sysfs/cgroupfs, but must not define cgroup, namespace, device, filesystem, network, scheduler, fd, signal, wait, or exec semantics. Split growing init setup into focused modules.

## Build Efficiency

Scope: OrlixKernel/Linux, OrlixMLibC/mlibc, and OrlixOS/Coreutils.

Use Linux Kbuild, Meson/Ninja, Autotools/Make, optional `ccache` or `sccache`. Misses, absent tools, stale dirs, or changed inputs fall back to the owning build system.

Do not invent package managers, stores, freshness DBs, custom Python build caches, proof ladders, or stamp ladders to bless Linux, mlibc, or Coreutils outputs.

## Proof

Proof is the test suite: upstream Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils/upstream package tests, OrlixOS/XCTest runtime tests, hosted terminal proofs, and Linux-oracle comparisons.

Package, rootfs, and fixture assembly may exist only to launch those suites. They are not readiness proof, package management, or a separate conformance framework. OCI reports may claim only implemented behavior backed by tests. Parser rejections, fixtures, stamps, manifests, feature JSON, and build metadata are not runtime proof.

## Product Model

One OrlixKernel runs multiple Linux environments: default Orlix root, imported rootfs environments, and OCI-derived environments. OCI metadata may configure an environment, but Linux owns semantics. Product behavior must feel like a real Linux terminal on iOS, not a parser demo, host compatibility layer, or custom runtime facade.

## Non-Actions

Do not add Docker daemon, `runc`, Apple Containerization runtime, Virtualization.framework, Linux VM lifecycle, `vminitd`, `vmnet`, Rosetta, custom OCI syscalls, HostAdapter Linux semantics, Darwin/Foundation/POSIX host APIs in OrlixKernel, generated upstream edits, mlibc patches masking Linux defects, proof systems, stamp ladders, feature-report-only work, or duplicated OrlixOS Linux subsystems.
