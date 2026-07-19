---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-19
summary: "Release and development profiles may differ in diagnostics, but their Linux UAPI, package ABI, device shape, and userspace contract remain invariant across supported destinations."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Profiles, ABI, and destination parity

The guest AArch64 ISA and Linux HWCAP declaration are part of the invariant userspace contract. A build profile or destination cannot advertise an optional ISA extension independently of the authoritative `arch/orlix` guest profile and its complete TCTI implementation.

Release and development profiles may differ in diagnostics, but their Linux UAPI, package ABI, device shape, and userspace contract remain invariant across supported destinations.
