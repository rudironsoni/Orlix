---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-09-10
summary: "OrlixKit is the public SDK over Apple-native runtime implementation and packaged Linux guest distribution artifacts."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Component ownership

`OrlixKit.xcframework` is the public SDK. It exposes OrlixEngine, OrlixOS, OrlixInstance, OrlixProcess, and OrlixContainer. It packages OrlixOS guest distribution resources without turning them into Apple-native link dependencies.

OrlixEngine owns process-wide host lifecycle. OrlixBootloader owns the private native boot mechanism. OrlixOS is the running hosted Linux operating system and uses one upstream Linux kernel. OrlixInstance owns persistent isolated Linux userspace state inside that OS.

Private static `OrlixKernel.xcframework` is upstream Linux plus the `arch/orlix` port. Linux remains the runtime and owns VMAs, page tables, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics.

`OrlixTCTI` is the private `arch/orlix` guest instruction-execution component inside OrlixKernel. It must implement the complete pinned 4,350-leaf AArch64 target, including the union of applicable EL0 feature configurations, while the runtime HWCAP and HWCAP2 projection controls only what Linux may advertise. OrlixTCTI does not replace Linux or reimplement Linux behavior. It returns structured exits to existing Linux-owned syscall, fault, signal, and scheduling paths. The hybrid boundary is defined by [ADR 0022](../objects/architecture-decision/0022-use-hosted-linux-elf-execution.md) and [ADR 0029](../objects/architecture-decision/0029-separate-complete-aarch64-target-from-runtime-profile.md).

`OrlixMLibC` owns libc sysdeps and consumes Linux UAPI. `OrlixCoreUtils` owns GNU Coreutils userspace packaging, not libc or kernel semantics. Guest packages and rootfs/images are distribution artifacts. None of these guest artifacts is an Apple-native private link dependency. `OrlixHostAdapter` owns private Apple host execution mechanics only; it cannot decode guest instructions or own Linux syscall policy. The native app owns Apple presentation, Ghostty rendering, and product composition, while Herdr owns terminal topology. Visibility and package identity are fixed by [ADR 0040](../objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md).
