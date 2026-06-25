# Goal

Deliver real OCI/container-image-derived Linux environments inside Orlix beside the default Orlix Linux environment. This is feature delivery, not rejection coverage. Parser rejections are temporary truth boundaries while implementation catches up. Push toward a capable Linux terminal on iOS with OCI roots, Linux process execution, PTYs, filesystems, namespaces, cgroups, networking, devices, `orlix run`, and registry input.

## Architecture

- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, the Orlix arch port, Linux-native drivers, boot code, or narrow Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. It must not own Linux policy, ABI, syscalls, package behavior, OCI lifecycle, cgroups, namespaces, filesystems, shells, or runtime.
- OrlixMLibC is userspace libc. It should stay unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux-surface issue, fix the Linux surface.
- OrlixOS is the Kit/session layer. It owns OCI import, descriptors, rootfs/package assembly, payload metadata, environment selection, lifecycle records, and app-facing APIs.
- Do not recreate OrlixKit, revive retired local-kernel paths, invent Orlix ABIs, clone Linux UAPI, or expose Darwin-shaped public surfaces.
- Virtio is an internal device plane below Linux subsystems, not public ABI, VM lifecycle, or runtime substitute.

## Linux Surface

Implement cgroups, namespaces, devices, mounts, procfs/sysfs/devfs/devpts, sockets, signals, wait, fd tables, exec, interpreter behavior, and filesystem semantics through Linux-owned code and Linux-visible interfaces. Do not duplicate these as OrlixOS, HostAdapter, Swift, package, fixture, or feature-report subsystems. Host adaptation stays private in HostAdapter or the Orlix port layer and invisible above OrlixKernel.

## Init Boundary

`OrlixOS/Sources/init/init.c` is bootstrap/orchestrator code, not the home for every OCI setup concern. Split growing setup into focused init-side modules for namespaces, cgroups, devices, mounts, process, terminal, lifecycle, and OCI setup. Those modules may call Linux syscalls and use procfs/sysfs/devfs/cgroupfs, but must not define Linux semantics or move policy into OrlixOS.

## Build Efficiency

Build-speed work must cover OrlixKernel/Linux, OrlixMLibC/mlibc, and OrlixOS/Coreutils. Use owning build-system incrementality and proven accelerators: Kbuild, Meson/Ninja, Autotools/Make, and optional `ccache`/`sccache` with safe fallback. Do not invent package managers, proof packages, stamp ladders, freshness databases, or custom correctness metadata.

## Proof

The proof is the test suite: upstream Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils/upstream package tests, OrlixOS/XCTest runtime tests, and Linux-oracle comparisons. Package/rootfs/test-fixture assembly is only setup required to launch those suites. It must not become a package manager, proof-package system, stamp ladder, freshness database, or independent proof framework. OCI feature reports may claim only behavior backed by implemented, relevant tests.

## Non-Actions

Do not add Docker daemon, `runc`, Apple Containerization runtime, Virtualization.framework, Linux VM lifecycle, `vminitd`, `vmnet`, Rosetta, custom OCI syscalls, HostAdapter-owned Linux semantics, Darwin/Foundation/POSIX host APIs in OrlixKernel, generated upstream edits, mlibc patches masking Linux defects, proof-package/stamp-ladder systems, or feature-report-only implementations.

## Product Model

One OrlixKernel runs multiple Linux environments: default Orlix root, imported rootfs environments, and OCI-derived environments. OCI metadata may configure an environment, but Linux semantics remain owned by Linux. The product must feel like a capable Linux terminal on iOS, not a parser demo, unsupported-field report, or host-side simulation.

## Goal Size

Keep `GOAL.md` under 4000 characters. Put details, proof logs, checkpoint history in `PLAN.md` and `IMPLEMENT.md`.
