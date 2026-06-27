# Goal
Deliver OCI-derived Linux environments beside the default Orlix root. The product must feel like a real, capable Linux terminal on iOS: Linux exec, PTYs/inherited stdio, filesystems, namespaces, cgroups, networking, devices, `orlix run`, registry input, lifecycle, and package workflows run through Linux behavior.

This is feature delivery. Parser rejections are temporary truth boundaries while implementation catches up. Do not optimize rejection coverage, unsupported-field reports, proof packages, stamp ladders, host simulation, or documentation theater.

## Reference Model
Use Docker, the OCI specs, Apple `container`, and Apple `containerization` as design references, adapted to Orlix rules. Follow their proven separations and UX concepts: image reference, pull, unpack/materialize, content-addressed inputs, bundle/config, create, start, exec, kill, wait, state, delete, healthcheck, typed process I/O, signals, events, and lifecycle records.

Do not import their incompatible runtime architecture. No Docker daemon, `runc`, Apple Containerization runtime dependency, Virtualization.framework, VM-per-container model, `vminitd`, `vmnet`, vsock runtime API, Rosetta, or custom Orlix ABI.

## Ownership
- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, Orlix arch port, Linux-native drivers, boot code, or Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. It must not own Linux policy, ABI, syscalls, packages, OCI lifecycle, cgroups, namespaces, filesystems, shells, or runtime semantics.
- OrlixMLibC consumes Linux. Keep it unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux-surface issue, fix Linux.
- OrlixOS is the Kit/session layer. It owns OCI import, descriptors, rootfs/package assembly, payload metadata, environment selection, lifecycle records, app APIs, and Linux-visible orchestration.
- Virtio is internal device plumbing below Linux subsystems. It is not public ABI, VM lifecycle, runtime substitute, Linux policy copy, or a Linux-semantic clone.

OrlixOS may preserve OCI intent and pass Linux-visible config to sessions, but it must not decide Linux semantics: cgroups, namespaces, mounts, devices, signals, wait/reaping, fd tables, procfs, sysfs, devtmpfs, devpts, sockets, exec, interpreters, permissions, syscalls, or package behavior. Missing behavior belongs in OrlixKernel or Linux-facing port code, not Swift, HostAdapter, fixtures, reports, or metadata.

## Build Efficiency
For OrlixKernel/Linux, OrlixMLibC/mlibc, and Coreutils, use proven build-system mechanisms: Linux Kbuild, Meson/Ninja, Autotools/Make, and optional `ccache`/`sccache`. Caches accelerate work only. They are not proof. Cache misses, absent tools, stale dirs, or changed inputs fall back to the owning build. Do not invent package managers, stores, freshness DBs, Python caches, proof ladders, or stamp ladders.

## Proof
Implement and prove behavior through Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils/upstream package tests, OrlixOS/XCTest runtime tests, hosted terminal proofs, and Linux-oracle comparisons. Package/rootfs/fixture assembly may exist only to launch those suites. It is not readiness proof, package management, stamp tracking, fresh DB, or a separate conformance framework.

Simulator/app-hosted tests must prove claimed product runtime behavior. Mocked tests alone are insufficient. OCI reports may claim only implemented behavior backed by relevant tests. Parser rejections, fixtures, stamps, manifests, feature JSON, build metadata, and generated reports are not runtime proof.

## Product Model
One OrlixKernel runs the default root, imported roots, and OCI-derived environments. Deliver a first-class terminal and runtime surface, not a parser demo, host compatibility layer, custom runtime facade, fake OrlixOS Linux subsystem, or shallow unsupported-field matrix.
