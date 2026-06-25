# Goal

Deliver OCI/container-image-derived Linux environments inside Orlix beside default Orlix Linux root. Outcome: a capable Linux terminal on iOS that runs OCI-derived roots with Linux process execution, PTYs or inherited stdio, filesystems, namespaces, cgroups, networking, devices, `orlix run`, registry input, and lifecycle.

This is feature delivery. Parser rejections are temporary truth boundaries while implementation catches up. Do not optimize for rejection coverage, feature-report theater, proof packages, stamp ladders, host simulation, or unsupported-surface reports.

## Architecture

- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, the Orlix arch port, Linux-native drivers, boot code, or narrow Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. It must not own Linux policy, ABI, syscalls, package behavior, OCI lifecycle, cgroups, namespaces, filesystems, shells, or runtime.
- OrlixMLibC consumes the Linux surface. Keep it unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux-surface issue, fix Linux.
- OrlixOS is the Kit/session layer. It owns OCI import, descriptors, rootfs/package assembly, payload metadata, environment selection, lifecycle records, app-facing APIs, and Linux-visible orchestration.
- Virtio is internal device plumbing below Linux subsystems. It is not public ABI, VM lifecycle, runtime substitute, Linux policy copy, or a Linux-semantic clone.

Do not recreate OrlixKit, revive retired local-kernel paths, invent Orlix ABIs, clone Linux UAPI, expose Darwin-shaped public surfaces, or add HostAdapter Linux semantics.

## Linux Surface

Implement cgroups, namespaces, devices, mounts, procfs, sysfs, devtmpfs, devpts, sockets, signals, wait/reaping, fd tables, exec, interpreters, filesystems, terminals, resources, networking, and lifecycle through Linux-owned code.

No duplicated Linux subsystems in OrlixOS, HostAdapter, Swift, fixtures, feature reporters, package metadata, or helper scripts. OrlixOS may configure environments and prepare rootfs payloads, but must not enforce Linux semantics. Host adaptation stays invisible above OrlixKernel.

## Build Efficiency

Scope: OrlixKernel/Linux, OrlixMLibC/mlibc, and OrlixOS/Coreutils. Use proven build-system mechanisms: Linux Kbuild, Meson/Ninja, Autotools/Make, and optional `ccache` or `sccache`. Cache misses, absent tools, stale dirs, or changed inputs fall back to the owning build system. Do not invent package managers, stores, freshness databases, Python build caches, proof ladders, or stamp ladders.

## Proof

Proof is the test suite: upstream Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils/upstream package tests, OrlixOS/XCTest runtime tests, hosted terminal proofs, and Linux-oracle comparisons. Package, rootfs, and fixture assembly may exist only to launch those suites. They are not readiness proof, package management, stamp tracking, freshness DBs, or a separate conformance framework.

OCI reports may claim only implemented behavior backed by relevant tests. Parser rejections, fixtures, stamps, manifests, feature JSON, build metadata, and generated reports are not runtime proof.

## Product Model

One OrlixKernel runs multiple Linux environments: default Orlix root, imported rootfs environments, and OCI-derived environments. OCI metadata may configure an environment, but Linux owns semantics. Product behavior must feel like a real Linux terminal on iOS, not a parser demo, host compatibility layer, unsupported-field report, or custom runtime facade.

## Non-Actions

Do not add Docker daemon, `runc`, Apple Containerization runtime, Virtualization.framework, Linux VM lifecycle, `vminitd`, `vmnet`, Rosetta, custom OCI syscalls, HostAdapter Linux semantics, Darwin/Foundation/POSIX host APIs in OrlixKernel, generated upstream edits, mlibc patches masking Linux defects, proof/stamp ladder systems, feature-report-only implementations, or duplicated OrlixOS Linux subsystems.
