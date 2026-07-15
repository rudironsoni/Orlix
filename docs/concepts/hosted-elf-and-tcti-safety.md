---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-15
summary: "Hosted Linux ELF execution preserves Linux process and ABI semantics while TCTI enforces translated execution constraints required by Apple platforms."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Hosted ELF and TCTI safety

Hosted Linux ELF execution preserves Linux process and ABI semantics while TCTI enforces translated execution constraints required by Apple platforms. JIT, RWX, and private-host shortcuts are not assumed.
