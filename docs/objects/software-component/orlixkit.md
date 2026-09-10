---
type: software-component
tags:
  - architecture
  - ownership
  - sdk
  - public
updated: 2026-09-10
status: active
summary: "Public embeddable Swift SDK and XCFramework that exposes OrlixEngine and packages the OrlixOS guest distribution resources."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixKit

`OrlixKit` is the public embeddable Swift module and XCFramework. It is the packaging and API boundary for the local Linux runtime, not a second terminal application and not a runtime object.

OrlixKit exposes `OrlixEngine`, `OrlixOS`, `OrlixInstance`, `OrlixProcess`, and `OrlixContainer`. Its private Apple-native implementation contains Engine, Bootloader, HostAdapter, and Kernel Mach-O integration. Its packaged guest resources contain the OrlixDistribution artifacts produced by OrlixMLibC, OrlixCoreUtils, guest packages, and rootfs assembly.

Host applications use OrlixKit as their only local Linux runtime dependency. The first-party Orlix application keeps its terminal, vvterm-derived surface, Ghostty integration, remote connections, and other application features outside OrlixKit.

This component page records the target boundary from [ADR 0040](../architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md). The current source graph remains under migration until its required dependency and runtime gates pass.
