---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-09-10
status: active
summary: "Private static mlibc port using installed Linux UAPI and Linux-shaped syscalls."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixMLibC

mlibc port built as a Linux guest userspace artifact from installed Linux UAPI and Linux-shaped syscalls. It is packaged or referenced by OrlixKit's OrlixDistribution resources, not exposed as an Apple-native link dependency or public SDK.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
