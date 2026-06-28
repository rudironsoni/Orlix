# Goal

Ship the first TestFlight-ready Orlix beta while continuing OCI-derived Linux environment work on the correct architecture.

Orlix must feel like a real Linux terminal on iOS: open to an interactive terminal, boot the delivered OrlixOS payload, run Linux userspace through OrlixKernel and OrlixMLibC, and tie claims to behavior proven on Simulator or device.

## Beta Deliverable

The beta priority is delivery readiness, not perfection. The repository must build from Xcode and command line using `project.yml` as source of truth, produce `Orlix` with bundle id `com.rudironsoni.Orlix`, embed `OrlixOS` correctly under Xcode user-script sandboxing, archive/sign when Apple signing assets are valid, and be ready for TestFlight.

The first screen is the terminal. Users must not see harness tests in the installed app. Test runners, conformance schemes, and proof fixtures remain separate from product and named by Orlix architecture.

## Architecture

- `Orlix` is the iOS app and terminal surface.
- `OrlixOS` is the Kit/distro/session/payload layer. It owns rootfs assembly, OCI import/materialization, environment selection, lifecycle records, and app-facing session APIs.
- `OrlixKernel` is upstream Linux plus the Orlix arch/device port. Linux behavior belongs in Linux, not OrlixOS or OrlixHostAdapter.
- `OrlixHostAdapter` owns private iOS/Darwin mechanics only. It must not expose Linux ABI, policy, cgroups, namespaces, filesystems, packages, or OCI runtime semantics.
- `OrlixMLibC` consumes the Linux surface. If mlibc exposes a Linux-surface issue, fix the Linux surface. Keep mlibc unpatched unless the defect is truly libc-owned.

Do not recreate `OrlixKit`, custom Linux facades, fake cgroups, fake drivers, custom ABI, custom package managers, proof packages, stamp ladders, freshness DBs, or host simulations that replace Linux behavior.

## OCI MVP Direction

Aim to include an OCI MVP in the first beta if it does not block app delivery. Follow proven Docker/OCI and Apple container/containerization concepts where they fit Orlix: image reference, pull/import, unpack/materialize, descriptor/config, create/start/exec/kill/wait/state/delete, typed stdio, signals, lifecycle records, and truthful feature reporting.

Adapt concepts, not incompatible runtime architecture. Do not add Docker daemon, `runc`, Apple runtime dependencies, Virtualization.framework, VM-per-container design, `vminitd`, `vmnet`, Rosetta, vsock runtime API, or custom Orlix ABI.

## Build And Project Rules

Use battle-tested build mechanisms only: Linux Kbuild, Meson/Ninja, Autotools/Make, XcodeGen, Xcode, and optional `ccache`/`sccache`. Caches accelerate work only. They are never proof.

`project.yml` is authoritative for Xcode targets, schemes, bundle ids, signing, script inputs/outputs, and generated project shape. Do not hand-edit generated Xcode projects as durable fixes.

Payload embedding must declare every source and destination required by Xcode user-script sandboxing, including child paths such as `Info.plist`, `.orlix-payload-ready`, `rootfs`, and `arch`.

## Proof

Behavior must be proven with the real test suite and real app-hosted execution. Mocked tests alone are insufficient.

Acceptable evidence includes Linux/kselftest, OrlixKernel probes, OrlixMLibC tests, Coreutils/upstream package tests, OrlixOS/XCTest runtime tests, hosted terminal proofs, Linux-oracle comparisons, XcodeGen validation, Xcode build/archive validation, and app launch checks.

Do not claim runtime readiness from parser rejection coverage, generated reports, fixture assembly, stamps, manifests, build metadata, or package/rootfs construction alone.

## Deferred After First Beta

After TestFlight beta publication, continue deeper OCI work: richer runtime spec coverage, registry UX, host-folder mounts, virtio-fs, virtio-net, DNS/NAT, namespaces, cgroup v2 controllers, resource accounting, broader package workflows, native performance benchmarks, and expanded Linux oracle coverage.
