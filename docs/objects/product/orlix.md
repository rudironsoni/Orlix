---
type: product
tags:
  - orlix
  - linux
updated: 2026-09-10
status: active
summary: "A curated upstream Linux system hosted by native Apple-platform applications."
has_part:
  - "[Orlix native app](../software-component/orlix-native-app.md)"
  - "[OrlixKit](../software-component/orlixkit.md)"
  - "[OrlixEngine](../software-component/orlixengine.md)"
  - "[OrlixBootloader](../software-component/orlixbootloader.md)"
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

Orlix is the first-party product and native application. Its public embeddable SDK is `OrlixKit.xcframework`. OrlixKit exposes OrlixEngine, OrlixOS, OrlixInstance, OrlixProcess, and OrlixContainer while keeping Apple-native implementation and Linux guest distribution resources distinct. Linux userspace remains normal Linux userspace, and Apple-platform mechanics remain private OrlixHostAdapter integration.
