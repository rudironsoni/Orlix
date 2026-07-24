---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-24
status: active
summary: "Upstream Linux port compiled as an iOS-hosted framework."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixKernel

Upstream Linux port compiled as an iOS-hosted framework.

OrlixKernel remains the Linux runtime. Its `arch/orlix` TCTI backend must execute the complete pinned 4,350-leaf AArch64 target, including the union of applicable EL0 feature configurations, when iOS cannot execute guest ELF text. The narrower runtime HWCAP and HWCAP2 projection controls only what Linux may advertise, not the completion denominator. Linux still owns memory management, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics. TCTI returns structured execution exits into those existing kernel paths instead of reimplementing them.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
