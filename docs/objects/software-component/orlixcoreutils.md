---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-09-10
status: active
summary: "GNU Coreutils Linux userspace artifact packaged in OrlixDistribution resources."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixCoreUtils

GNU Coreutils built for Orlix Linux userspace and packaged into OrlixDistribution resources for OrlixKit. It is not an Apple-native link dependency or public SDK, and it does not own libc, kernel behavior, or a Darwin command facade.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
