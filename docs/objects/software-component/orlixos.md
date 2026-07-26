---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-26
status: active
summary: "Sole public SDK for curated Linux distribution, OrlixMachine sessions, and OrlixOS.Containers."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixOS

`OrlixOS.xcframework` is the sole public SDK, with identifier `com.rudironsoni.orlix.os`. It owns curated Linux distribution resources directly, app-facing `OrlixMachine` sessions, and the `OrlixOS.Containers` namespace. It does not ship a separate payload bundle or expose the private static implementation artifacts as public SDKs.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
