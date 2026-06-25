# Goal

Deliver real OCI/container-image-derived Linux environments inside Orlix, beside the default Orlix Linux environment. This is feature delivery, not rejection coverage. Parser rejections are temporary truth boundaries only; work must push toward a capable Linux terminal on iOS with OCI roots, Linux process execution, PTYs, filesystems, namespaces, cgroups, networking, devices, `orlix run`, and registry input.

## Architecture

- OrlixKernel is Linux. Linux behavior belongs in upstream Linux, the Orlix arch port, Linux-native drivers, boot code, or narrow Linux-facing port glue.
- OrlixHostAdapter owns private iOS/Darwin mechanics only. It must not own Linux policy, ABI, syscalls, package behavior, OCI lifecycle, cgroups, namespaces, filesystems, shells, or public runtime behavior.
- OrlixMLibC is userspace libc. It should stay unpatched unless a defect is truly libc-owned. If mlibc exposes a Linux-surface issue, fix the Linux surface.
- OrlixOS is the Kit/session layer. It owns OCI import, descriptors, rootfs/package assembly, payload metadata, environment selection, lifecycle records, and app-facing session APIs.
- Do not recreate OrlixKit, revive retired local-kernel paths, invent Orlix ABIs, clone Linux UAPI locally, or expose Darwin-shaped public surfaces.
- Virtio is an internal device plane below Linux subsystems, not public userspace ABI, VM lifecycle, or runtime substitute.

## Init Boundary

`OrlixOS/Sources/init/init.c` is bootstrap/orchestrator code, not the long-term home for every OCI setup concern. Split growing setup into focused init-side modules for namespaces, cgroups, devices, mounts, process, terminal, lifecycle, and OCI setup. Those modules may call Linux syscalls and use procfs/sysfs/devfs/cgroupfs, but they must not define Linux semantics or move policy into HostAdapter.

## Proof And Build

Build-speed work must cover OrlixKernel/Linux, OrlixMLibC/mlibc, and OrlixOS/Coreutils. Use owning build systems and proven accelerators: Kbuild, Meson/Ninja, Autotools/Make, and optional `ccache`/`sccache` with safe fallback. Do not invent package managers, proof packages, stamp ladders, freshness databases, or custom manifests to decide outputs are correct.

The test suite is the proof surface. Package, rootfs, and fixture assembly are setup, not proof. Claims need evidence at the right layer: Linux/kselftest/oracle for kernel behavior, upstream mlibc tests for libc, upstream package or OrlixOS execution proof for packages, and app-hosted OrlixOS proof for product behavior. OCI feature reports may claim only implemented behavior with directly relevant proof.

## Non-Actions

Do not add Docker daemon, `runc`, Apple Containerization runtime, Virtualization.framework, Linux VM lifecycle, `vminitd`, `vmnet`, Rosetta, custom OCI syscalls, HostAdapter-owned Linux semantics, Darwin/Foundation/POSIX host APIs in OrlixKernel, generated upstream edits, mlibc patches that mask Linux defects, proof-package systems, or feature-report-only implementations.

## Product Model

One OrlixKernel runs multiple Linux environments: default Orlix root, imported rootfs environments, and OCI-derived environments. OCI metadata may configure an environment, but Linux semantics remain owned by Linux. The final product must feel like a capable Linux terminal on iOS, not a parser demo or unsupported-field report.
