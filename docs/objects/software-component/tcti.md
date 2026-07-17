---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-17
status: active
summary: "Hosted translated code transfer interpreter used for safe Linux ELF execution."
part_of:
  - "[Orlix](../product/orlix.md)"
derived_from:
  - "[TCTI reference review](../../sources/tcti/reference-review.md)"
---

# TCTI

Hosted translated code transfer interpreter used for safe Linux ELF execution.

TCTI owns complete ISA-on-ISA execution for every architecturally valid AArch64 EL0 instruction in the guest-exposed ISA profile. Its production path uses Orlix-owned fetch, decode, lowering, data-only gadget dispatch, register state, memory access, and structured exits under `arch/orlix`. Instruction subsets trimmed to a package workload, exact-opcode production special cases, host-native guest execution, and silent semantic approximations are not valid completion strategies.

KUnit owns decode, lowering, exact state transition, exception, and reserved-encoding proof. Linux kselftest owns live Linux-visible execution behavior. Package suites are downstream compatibility evidence and cannot substitute for complete ISA coverage.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
