---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-19
status: active
summary: "Upstream Linux port compiled as an iOS-hosted framework."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixKernel

Upstream Linux port compiled as an iOS-hosted framework.

OrlixKernel remains the Linux runtime. Its `arch/orlix` TCTI backend executes the complete guest-exposed AArch64 EL0 instruction profile when iOS cannot execute guest ELF text, but Linux still owns memory management, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics. TCTI returns structured execution exits into those existing kernel paths instead of reimplementing them.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
