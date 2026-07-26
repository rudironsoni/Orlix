---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-26
summary: "Orlix is the product and app, OrlixOS is the sole public SDK, OrlixMachine is its local Linux lifecycle type, and implementation layers remain private."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Orlix domain language

`Orlix` is the product and native application. `OrlixOS.xcframework` is the sole public SDK. `OrlixMachine` is its local Linux lifecycle type, and `OrlixOS.Containers` is its container namespace.

`OrlixKernel`, `OrlixMLibC`, and `OrlixCoreUtils` are private static implementation artifacts. `OrlixHostAdapter` is private Apple execution integration. `OrlixTCTI` is the private guest instruction-execution component under `arch/orlix`.

Herdr owns Session, Workspace, Tab, and Pane names and topology. `OrlixTestApp` and `OrlixOSTestApp` are private test hosts. Retired module, target, lifecycle, and payload names have no compatibility aliases.

The owning packaging and visibility decision is [ADR 0030](../objects/architecture-decision/0030-use-one-public-orlixos-sdk-and-private-static-implementation-layers.md).
