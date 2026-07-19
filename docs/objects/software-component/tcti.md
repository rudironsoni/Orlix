---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-19
status: active
summary: "Hosted translated code transfer interpreter used for safe Linux ELF execution."
part_of:
  - "[Orlix](../product/orlix.md)"
derived_from:
  - "[TCTI reference review](../../sources/tcti/reference-review.md)"
---

# TCTI

The authoritative current guest profile is declared by `arch/orlix/include/asm/isa.h`: Armv8.0-A with floating point and AdvSIMD, exposed to Linux userspace as `HWCAP_FP | HWCAP_ASIMD` and no `HWCAP2` features. The declaration must match the userspace compiler target. An optional extension may be advertised only after its complete EL0 instruction families, legal encoding boundaries, architectural semantics, and deterministic exception behavior are covered by TCTI KUnit.

Hosted translated code transfer interpreter used for safe Linux ELF execution.

TCTI owns complete ISA-on-ISA execution for every architecturally valid AArch64 EL0 instruction in the guest-exposed ISA profile. Its production path uses Orlix-owned fetch, decode, lowering, data-only gadget dispatch, register state, memory access, and structured exits under `arch/orlix`. Instruction subsets trimmed to a package workload, exact-opcode production special cases, host-native guest execution, and silent semantic approximations are not valid completion strategies.

TCTI is not a second kernel, a Linux syscall emulator, or the general Orlix runtime. OrlixKernel continues to execute Linux kernel code natively and owns VMAs, page tables, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics. TCTI exists only at the guest AArch64 EL0 instruction boundary forced by iOS executable-memory restrictions. A guest `svc #0`, fault, signal check, or yield exits TCTI into the corresponding Linux-owned `arch/orlix` path, as defined by [ADR 0022](../architecture-decision/0022-use-hosted-linux-elf-execution.md).

KUnit owns decode, lowering, exact state-transition, exception, and reserved-encoding proof. Linux kselftest owns live Linux-visible execution behavior. Package suites are downstream compatibility evidence and cannot substitute for complete ISA coverage.

The authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
