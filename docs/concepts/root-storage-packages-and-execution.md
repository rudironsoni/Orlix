---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-09-10
summary: "OrlixDistribution assembles guest root filesystem and package resources for OrlixKit to package with OrlixOS."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Root storage, packages, and execution

`OrlixDistribution` assembles curated root filesystems and packages as Linux guest resources for `OrlixKit`. OrlixOS is the running hosted OS, not the rootfs artifact. Linux mounts and executes the resources through upstream storage, VFS, `exec`, and interpreter mechanisms.
