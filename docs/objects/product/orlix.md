---
type: product
tags:
  - orlix
  - linux
updated: 2026-07-26
status: active
summary: "A curated upstream Linux system hosted by native Apple-platform applications."
has_part:
  - "[Orlix native app](../software-component/orlix-native-app.md)"
  - "[OrlixOS](../software-component/orlixos.md)"
  - "[OrlixKernel](../software-component/orlixkernel.md)"
  - "[OrlixMLibC](../software-component/orlixmlibc.md)"
  - "[OrlixCoreUtils](../software-component/orlixcoreutils.md)"
  - "[OrlixHostAdapter](../software-component/orlixhostadapter.md)"
  - "[OrlixTCTI](../software-component/orlixtcti.md)"
  - "[Agent harness](../software-component/agent-harness.md)"
  - "[Release harness](../software-component/release-harness.md)"
---

# Orlix

Orlix is the product and native application. Its sole public SDK is `OrlixOS.xcframework` with identifier `com.rudironsoni.orlix.os`. The SDK delivers `OrlixMachine` sessions and the `OrlixOS.Containers` namespace over private static `OrlixKernel`, `OrlixMLibC`, and `OrlixCoreUtils` implementation artifacts. Linux userspace remains normal Linux userspace, and Apple-platform mechanics remain private `OrlixHostAdapter` integration.
