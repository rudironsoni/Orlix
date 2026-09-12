---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-09-10
summary: "OrlixKit is the public SDK over private Apple-native implementation and packaged Linux guest distribution resources."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# App-hosted build and packaging

The product exposes `OrlixKit.xcframework`. Its Apple-native implementation contains OrlixEngine, OrlixBootloader, OrlixHostAdapter, and Kernel Mach-O integration. Its guest distribution resources contain the outputs of OrlixMLibC, OrlixCoreUtils, guest packages, and rootfs assembly. Guest resources may be packaged or referenced by OrlixKit, but they are not Apple-native link dependencies and do not redefine Linux semantics.

`OrlixTestApp` hosts lower implementation-layer proof. `OrlixOSTestApp` consumes OrlixKit while hosting the running OrlixOS product-session path. Neither is a consumer SDK or product identity. [ADR 0040](../objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md) owns these names and visibility boundaries.
