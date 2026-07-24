---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-23
summary: "OrlixKernel owns Linux and TCTI guest instruction execution, OrlixMLibC owns libc sysdeps, OrlixHostAdapter owns private Apple host mechanics, OrlixOS owns distribution and the app-facing session surface, and the native app owns presentation."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Component ownership

OrlixKernel is upstream Linux plus the `arch/orlix` port. Linux remains the runtime and owns VMAs, page tables, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics.

TCTI is an `arch/orlix` guest instruction-execution backend inside OrlixKernel. It must implement the complete pinned 4,350-leaf AArch64 target, including the union of applicable EL0 feature configurations, while the runtime HWCAP and HWCAP2 projection controls only what Linux may advertise. TCTI does not replace Linux or reimplement Linux behavior. It executes guest instructions while iOS prevents guest ELF text from becoming host executable, then returns structured exits to the existing Linux-owned syscall, fault, signal, and scheduling paths. This hybrid boundary is defined by [ADR 0022](../objects/architecture-decision/0022-use-hosted-linux-elf-execution.md) and [ADR 0029](../objects/architecture-decision/0029-separate-complete-aarch64-target-from-runtime-profile.md).

OrlixMLibC owns libc sysdeps and consumes the Linux UAPI. OrlixHostAdapter owns private Apple host mechanics only and cannot decode guest instructions or Linux syscall policy. OrlixOS owns curated distribution policy, payload assembly and metadata, and the app-facing Linux session surface. The native app owns presentation, Ghostty, UIKit and SwiftUI lifecycle, windows, tabs, panes, and app-level composition.
