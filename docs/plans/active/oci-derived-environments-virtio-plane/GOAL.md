# Goal

Deliver real OCI/container-image-derived Linux environments inside Orlix beside the default Orlix Linux root. The outcome is a capable Linux terminal on iOS that can run OCI-derived roots through Linux process execution, PTYs, filesystems, namespaces, cgroups, networking, devices, `orlix run`, and registry input.

This goal is feature delivery. Do not spend effort on rejection coverage, parser theater, proof packages, stamp ladders, host simulation, or reports that make unsupported behavior look like progress.

## Architecture

- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, the Orlix arch port, Linux-native drivers, boot code, or narrow Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. Host adaptation must remain invisible to userspace above OrlixKernel and must not own Linux policy, ABI, syscalls, packages, cgroups, namespaces, filesystems, shells, or runtime semantics.
- OrlixMLibC is userspace libc. Keep it unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux-surface issue, fix the Linux surface.
- OrlixOS is the Kit/session layer. It owns OCI import, descriptors, rootfs assembly, payload metadata, environment selection, lifecycle records, app-facing APIs, and Linux setup orchestration through Linux-visible interfaces.
- Do not recreate OrlixKit, revive retired local-kernel paths, invent Orlix ABIs, clone Linux UAPI, or expose Darwin-shaped public surfaces.
- Virtio is an internal device plane below Linux subsystems, not public ABI, VM lifecycle, runtime substitute, or Linux policy copy.

## Linux Surface

Implement and fix cgroups, namespaces, devices, mounts, procfs, sysfs, devfs, devpts, sockets, signals, wait/reaping, fd tables, exec, interpreters, filesystem semantics, terminal behavior, resource control, and networking through Linux-owned code and Linux-visible interfaces.

Do not duplicate Linux subsystems in OrlixOS, Swift, XCTest fixtures, HostAdapter, or package harness code. OrlixOS may orchestrate by invoking Linux syscalls and writing Linux filesystems such as procfs, sysfs, cgroupfs, devfs, and devpts, but the semantics must remain owned by Linux.

The implementation must support both terminal and non-terminal OCI process modes without custom runtime facades. `process.terminal=true` uses Linux PTY behavior. `process.terminal=false` must run on inherited stdio with the same Linux setup path.

## Proof

Proof is the test suite: upstream Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils and upstream package tests, OrlixOS/XCTest runtime tests, hosted terminal proofs, and Linux-oracle comparisons. Package, rootfs, and fixture assembly may exist only to launch those suites.

OCI feature reports may claim only implemented behavior backed by tests. Parser rejections, fixtures, stamps, package manifests, feature JSON, and build metadata are not readiness proof.

## Product Model

One OrlixKernel runs multiple Linux environments: default Orlix root, imported rootfs environments, and OCI-derived environments. OCI metadata may configure an environment, but Linux semantics remain owned by Linux. Product behavior must feel like a real Linux terminal on iOS, not a parser demo, unsupported-field report, host compatibility layer, or custom runtime facade.

## Non-Actions

Do not add Docker daemon, `runc`, Apple Containerization runtime, Virtualization.framework, Linux VM lifecycle, `vminitd`, `vmnet`, Rosetta, custom OCI syscalls, HostAdapter-owned Linux semantics, Darwin/Foundation/POSIX host APIs in OrlixKernel, generated upstream edits, mlibc patches masking Linux defects, proof-package stamp-ladder systems, feature-report-only implementations, or duplicated OrlixOS cgroup/namespace/device/filesystem/network subsystems.

## Goal Size

Keep `GOAL.md` under 4000 characters. Put design detail, proof logs, checkpoints, and tactical order in `PLAN.md` and `IMPLEMENT.md`.
