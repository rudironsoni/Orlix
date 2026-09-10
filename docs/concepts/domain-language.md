---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-09-10
summary: "Orlix is the product and app, OrlixKit is the public SDK, OrlixEngine hosts one OrlixOS, and OrlixInstances own isolated Linux userspace."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Orlix domain language

`Orlix` is the first-party product and native application. `OrlixKit.xcframework` is the public embeddable SDK. `OrlixEngine` hosts one running `OrlixOS`; `OrlixInstance` is its persistent isolated Linux userspace; `OrlixProcess` is an ordinary Linux process; and `OrlixContainer` is an OCI container belonging to one instance.

`OrlixBootloader`, `OrlixKernel`, and `OrlixHostAdapter` are private Apple-native implementation layers. `OrlixMLibC`, `OrlixCoreUtils`, guest packages, and rootfs/images are Linux guest or distribution artifacts packaged by OrlixKit. `OrlixTCTI` is the private guest instruction-execution component under `arch/orlix`.

Herdr owns Session, Workspace, Tab, and Pane names and topology. `OrlixTestApp` and `OrlixOSTestApp` are private test hosts. Retired module, target, lifecycle, and payload names have no compatibility aliases.

The owning packaging and visibility decision is [ADR 0040](../objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md), which amends [ADR 0030](../objects/architecture-decision/0030-use-one-public-orlixos-sdk-and-private-static-implementation-layers.md).
