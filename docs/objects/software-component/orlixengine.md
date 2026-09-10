---
type: software-component
tags:
  - architecture
  - ownership
  - runtime
updated: 2026-09-10
status: active
summary: "Process-wide Apple-hosted lifecycle owner that boots and hosts one running OrlixOS."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixEngine

`OrlixEngine` owns process-wide host lifecycle, boot orchestration, host resource establishment, and access to the running `OrlixOS`. It hosts at most one running OrlixOS in an application process.

Engine boot identity includes every process-wide Kernel and boot-resource input. Instance rootfs, namespaces, processes, containers, and terminal configuration are instance or consumer state and do not change that identity. Compatible concurrent boot requests converge on one boot; incompatible requests fail without replacing the running OS.

OrlixEngine does not own Linux syscall, process, VFS, namespace, libc, package, or terminal UI policy. Those behaviors remain with upstream Linux, guest userspace, OrlixDistribution artifacts, and the first-party application as appropriate.

This component page records the target boundary from [ADR 0040](../architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md). Runtime lifecycle proof remains unfinished until the owning gates pass.
