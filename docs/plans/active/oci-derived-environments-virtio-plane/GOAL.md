# Goal

Deliver OCI-derived Linux environments beside the default Orlix root. Outcome: a capable Linux terminal on iOS runs OCI-derived roots with Linux exec, PTYs/stdio, filesystems, namespaces, cgroups, networking, devices, `orlix run`, registry input, and lifecycle.

This is feature delivery. Parser rejections are temporary truth boundaries while implementation catches up. Do not optimize for rejection coverage, report theater, proof packages, stamp ladders, host simulation, or unsupported reports.

## Ownership

- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, Orlix arch port, Linux-native drivers, boot code, or Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. It must not own Linux policy, ABI, syscalls, packages, OCI lifecycle, cgroups, namespaces, filesystems, shells, or runtime.
- OrlixMLibC consumes Linux. Keep it unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux-surface issue, fix Linux.
- OrlixOS is Kit/session layer. It owns OCI import, descriptors, rootfs/package assembly, payload metadata, environment selection, lifecycle records, app APIs, and Linux-visible orchestration.
- Virtio is internal device plumbing below Linux subsystems. It is not public ABI, VM lifecycle, runtime substitute, Linux policy copy, or Linux-semantic clone.

OrlixOS must not reinvent Linux. It may preserve OCI intent and pass Linux-visible config to sessions, but must not decide Linux semantics: cgroup controller/file support, namespaces, mounts, devices, signals, wait/reaping, fd tables, procfs, sysfs, devtmpfs, devpts, sockets, exec, interpreters, permissions, or syscalls. Missing behavior belongs in OrlixKernel or Linux-facing port code, not Swift, HostAdapter, fixtures, reports, or metadata.

## Linux Surface

Implement real Linux behavior for OCI-derived environments: cgroups, namespaces, devices, mounts, procfs, sysfs, devtmpfs, devpts, sockets, signals, wait/reaping, fd tables, exec, interpreters, filesystems, PTYs, and networking. OCI metadata configures environments; Linux owns behavior.

## Build Efficiency

Scope: OrlixKernel/Linux, OrlixMLibC/mlibc, and OrlixOS/Coreutils. Use Linux Kbuild, Meson/Ninja, Autotools/Make, and optional `ccache` or `sccache`. Cache misses, absent tools, stale dirs, or changed inputs fall back to the owning build. Do not invent package managers, stores, freshness DBs, Python caches, proof ladders, or stamp ladders.

## Proof

Proof is test-suite and runtime evidence: upstream Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils/upstream package tests, OrlixOS/XCTest runtime tests, hosted terminal proofs, and Linux-oracle comparisons. Package/rootfs/fixture assembly may exist only to launch those suites. It is not readiness proof, package management, stamp tracking, fresh DB, or separate conformance framework.

OCI reports may claim only implemented behavior backed by relevant tests. Parser rejections, fixtures, stamps, manifests, feature JSON, build metadata, and generated reports are not runtime proof. Simulator/app-hosted tests must prove claimed product runtime behavior. Mocked tests alone are insufficient.

## Product Model

One OrlixKernel runs default, imported rootfs, and OCI-derived environments. Product behavior must feel like a real Linux terminal on iOS, not a parser demo, host compatibility layer, unsupported-field report, custom runtime facade, or fake OrlixOS Linux subsystem.

## Non-Actions

Do not add Docker daemon, `runc`, Apple Containerization runtime, Virtualization.framework, Linux VM lifecycle, `vminitd`, `vmnet`, Rosetta, custom OCI syscalls, HostAdapter Linux semantics, Darwin/POSIX host APIs in OrlixKernel, generated upstream edits, mlibc patches masking Linux defects, proof/stamp ladders, feature-report-only work, duplicated OrlixOS Linux subsystems, OrlixKit revival, retired local-kernel paths, Orlix ABIs, UAPI clones, or Darwin-shaped public surfaces.
