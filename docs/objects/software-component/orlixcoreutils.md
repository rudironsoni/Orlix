---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-26
status: active
summary: "Private static GNU Coreutils userspace packaged for OrlixOS."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixCoreUtils

GNU Coreutils built for Orlix Linux userspace and packaged into private static `OrlixCoreUtils.xcframework` with identifier `com.rudironsoni.orlix.os.coreutils`. It is an implementation dependency of `OrlixOS.xcframework`, not a public SDK, libc, kernel behavior owner, or Darwin command facade.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
