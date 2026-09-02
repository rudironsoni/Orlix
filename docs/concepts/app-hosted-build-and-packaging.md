---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-08-31
summary: "OrlixOS is the sole public SDK over private static Mach-O-native implementation artifacts and private test hosts."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# App-hosted build and packaging

The product exposes only `OrlixOS.xcframework`. It statically consumes private Mach-O-native `OrlixKernel.xcframework`, `OrlixMLibC.xcframework`, and `OrlixCoreUtils.xcframework` artifacts and private `OrlixHostAdapter` execution integration without redefining Linux semantics. Curated distribution resources belong directly to OrlixOS; there is no separate payload bundle.

`OrlixTerminalTestApp` hosts the native terminal application tests. `OrlixOSTestApp` hosts only the OrlixOS Linux userspace and its private implementation-layer proof. Neither is a consumer SDK or product identity. [ADR 0030](../objects/architecture-decision/0030-use-one-public-orlixos-sdk-and-private-static-implementation-layers.md) owns these names and visibility boundaries.
