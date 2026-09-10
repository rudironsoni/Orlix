---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-09-10
status: accepted
external_id: "ADR-0004"
summary: "Keep the Linux boot-entry interface private behind OrlixKit's public lifecycle handles."
part_of:
  - "[Orlix](../product/orlix.md)"
amended_by:
  - "[ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md)"
---

# ADR 0004: Keep The Boot-Entry API Private

## Status

Accepted

## Context

The product API could expose raw boot parameters, syscall wrappers, runtime management functions, or a minimal bootloader entrypoint.

Exposing Linux management APIs from the product surface would confuse kernel, libc, userspace, and host boundaries.

## Decision

The app-facing product API lives in the public `OrlixKit.xcframework`. It exposes `OrlixEngine`, `OrlixOS`, `OrlixInstance`, `OrlixProcess`, and `OrlixContainer` handles with closed profile selection and opaque resource identifiers. The lower-level boot entrypoint remains private to OrlixBootloader and Kernel integration, and apps do not target Kernel headers directly. [ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md) defines the updated product boundary.

## Consequences

Raw `struct boot_params` is not the main public API.

Raw Linux syscall, fd-table, mount, execve, task-structure, and cgroup implementation interfaces remain private. OrlixKit exposes the Engine, instance, process, and OCI lifecycle handles defined by ADR 0040. Those handles do not transfer Linux semantics into a host-side facade.

OrlixKit packages or references curated OrlixDistribution resources and registers private HostAdapter resource paths before boot. OrlixOS is the running hosted OS, not the rootfs artifact. There is no target-selected mutable payload identity. The private OrlixBootloader translates process-wide app inputs into Linux-shaped boot data.
