---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-26
summary: "OrlixOS owns curated root filesystem and package resources directly inside its public SDK."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Root storage, packages, and execution

OrlixOS assembles curated root filesystems and packages as resources owned directly by `OrlixOS.xcframework`; there is no separate payload bundle. Linux mounts and executes them through upstream storage, VFS, exec, and interpreter mechanisms.
