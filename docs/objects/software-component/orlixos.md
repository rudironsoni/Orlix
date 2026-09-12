---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-09-10
status: active
summary: "Running hosted Linux operating system with one Kernel and persistent OrlixInstances."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixOS

`OrlixOS` is the running hosted Linux operating system. It uses one upstream Linux kernel and hosts zero or more persistent `OrlixInstance` userspaces containing ordinary `OrlixProcess` values and `OrlixContainer` workloads. OrlixDistribution assembles guest resources for OrlixKit; OrlixOS is not the rootfs artifact and is not the public SDK.

Its public packaging and ownership boundary is OrlixKit. Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md) and [ADR 0040](../architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md).
