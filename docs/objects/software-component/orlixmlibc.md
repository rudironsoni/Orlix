---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-26
status: active
summary: "Private static mlibc port using installed Linux UAPI and Linux-shaped syscalls."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixMLibC

mlibc port packaged into private static `OrlixMLibC.xcframework` with identifier `com.rudironsoni.orlix.os.mlibc`. It uses installed Linux UAPI and Linux-shaped syscalls and is not a public SDK.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
