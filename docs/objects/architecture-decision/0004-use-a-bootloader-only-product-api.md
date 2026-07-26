---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-26
status: accepted
external_id: "ADR-0004"
summary: "Durable Orlix architecture decision ADR 0004."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# ADR 0004: Use A Bootloader-Only Product API

## Status

Accepted

## Context

The product API could expose raw boot parameters, syscall wrappers, runtime management functions, or a minimal bootloader entrypoint.

Exposing Linux management APIs from the product surface would confuse kernel, libc, userspace, and host boundaries.

## Decision

The app-facing product API lives in the sole public `OrlixOS.xcframework`. It exposes `OrlixMachine` sessions with closed profile selection and opaque app-level resource identifiers; [ADR 0028](0028-provide-full-docker-engine-compatibility-through-orlixos.md) separately owns `OrlixOS.Containers`. The lower-level boot entrypoint remains under `OrlixKernel/Sources/include` for private kernel integration, and apps do not target kernel headers directly.

## Consequences

Raw `struct boot_params` is not the main public API.

Public syscall, file, mount, exec, task, cgroup, and runtime management APIs are forbidden.

`OrlixOS` resolves curated distribution resources from its own framework and registers private HostAdapter resource paths before boot. There is no separate payload bundle or target-selected payload identity. The bootloader under `OrlixKernel/Sources/boot` translates app-level inputs into Linux-shaped boot data.
