---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-09-09
summary: "Hosted Linux ELF execution preserves Linux process and ABI semantics while TCTI enforces translated execution constraints required by Apple platforms."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Hosted ELF and TCTI safety

Hosted Linux ELF execution preserves Linux process and ABI semantics while TCTI enforces translated execution constraints required by Apple platforms. JIT, RWX, and private-host shortcuts are not assumed.

Product TCTI runs guest A64 as cached data-only gadget programs. It does not generate executable memory. The translation unit is a straight-line basic block of C handlers, as decided in [ADR 0039](../objects/architecture-decision/0039-cache-tcti-basic-blocks-as-gadget-programs.md).
