---
type: software-component
tags:
  - architecture
  - ownership
  - runtime
updated: 2026-09-10
status: active
summary: "Private native boot mechanism that establishes the host environment required to enter the upstream Linux kernel."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixBootloader

`OrlixBootloader` is the private native boot mechanism behind OrlixEngine. It establishes the resources and narrow interfaces required to enter the Mach-O-linked upstream Linux Kernel integration.

It does not own Linux policy, libc behavior, package assembly, process lifecycle, terminal topology, or public syscall APIs. It consumes narrow Kernel and HostAdapter interfaces and remains independently incremental from guest distribution artifacts.

This component page records the target boundary from [ADR 0040](../architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md). The current boot glue remains a migration input until the private target and its proof gates are complete.
