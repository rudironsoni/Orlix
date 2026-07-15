---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-15
status: accepted
external_id: "ADR-0009"
summary: "Durable Orlix architecture decision ADR 0009."
part_of:
  - "[Orlix](../product/orlix.md)"
amended_by:
  - "[ADR 0023](0023-use-release-development-profiles-and-curated-orlixos-distribution.md)"
---

# ADR 0009: Support Serial And Virtio Console Selection

## Status

Accepted, updated by ADR 0023.

## Context

Orlix needs early boot diagnostics, fallback/debug console behavior, and a normal interactive virtual console path.

A single hardwired console would not match Linux boot expectations.

## Decision

Orlix supports both serial-style console behavior and upstream virtio-console. Boot-time selection follows normal Linux `console=` behavior.

## Consequences

The release profile enables both console paths.

The serial-style console is available for early, debug, or fallback use.

Virtio-console is the normal interactive direction where upstream Linux behavior fits.

`arch/orlix` code from `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix` may provide a minimal early console before normal console drivers register.
