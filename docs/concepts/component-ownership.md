---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-26
summary: "OrlixOS is the sole public SDK over private Linux, libc, Coreutils, TCTI, and Apple host-execution layers."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Component ownership

`OrlixOS.xcframework` is the sole public SDK. It owns curated distribution resources, the `OrlixMachine` session surface, and `OrlixOS.Containers`. It contains no separate payload bundle.

Private static `OrlixKernel.xcframework` is upstream Linux plus the `arch/orlix` port. Linux remains the runtime and owns VMAs, page tables, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics.

`OrlixTCTI` is the private `arch/orlix` guest instruction-execution component inside OrlixKernel. It must implement the complete pinned 4,350-leaf AArch64 target, including the union of applicable EL0 feature configurations, while the runtime HWCAP and HWCAP2 projection controls only what Linux may advertise. OrlixTCTI does not replace Linux or reimplement Linux behavior. It returns structured exits to existing Linux-owned syscall, fault, signal, and scheduling paths. The hybrid boundary is defined by [ADR 0022](../objects/architecture-decision/0022-use-hosted-linux-elf-execution.md) and [ADR 0029](../objects/architecture-decision/0029-separate-complete-aarch64-target-from-runtime-profile.md).

Private static `OrlixMLibC.xcframework` owns libc sysdeps and consumes Linux UAPI. Private static `OrlixCoreUtils.xcframework` owns GNU Coreutils userspace packaging, not libc or kernel semantics. `OrlixHostAdapter` owns private Apple host execution mechanics only; it cannot decode guest instructions or own Linux syscall policy. The native app owns Apple presentation, Ghostty rendering, and product composition, while Herdr owns terminal topology. Visibility and package identity are fixed by [ADR 0030](../objects/architecture-decision/0030-use-one-public-orlixos-sdk-and-private-static-implementation-layers.md).
