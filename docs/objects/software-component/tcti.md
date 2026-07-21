---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-22
status: active
summary: "Orlix-owned AArch64 EL0 interpreter for safe Linux ELF execution."
part_of:
  - "[Orlix](../product/orlix.md)"
derived_from:
  - "[TCTI reference review](../../sources/tcti/reference-review.md)"
---

# TCTI

The authoritative current guest profile is declared by `arch/orlix/include/asm/isa.h`: Armv8.0-A with floating point, AdvSIMD, AES, polynomial multiply, SHA-1, SHA-2, CRC32, SHA-3, SM3, SM4, and SHA-512, exposed to Linux userspace through their corresponding `HWCAP` bits with no `HWCAP2` features. This declaration must match the userspace compiler target. An optional extension may be advertised only after its complete EL0 instruction families, legal encoding boundaries, architectural semantics, and deterministic exception behavior are covered by TCTI KUnit.

`arch/orlix/hosted_exec/tcti/isa_coverage.h` owns the machine-readable instruction-family inventory. Coverage status is independent of decoder presence: a row remains partial until direct KUnit evidence covers production-path boundaries, state transitions, and exceptions. KUnit ratchets the remaining gap count to zero without treating planned test names as evidence.

The A64 `HLT` encoding is inventoried explicitly. Orlix does not expose external halting debug or semihosting, so TCTI reports `HLT` through structured unsupported-instruction state without advancing the instruction PC and Linux delivers `SIGILL`; `BRK` remains a structured breakpoint delivered as `SIGTRAP`.

TCTI owns complete ISA-on-ISA execution for every architecturally valid AArch64 EL0 instruction in the guest-exposed ISA profile. The production path uses Orlix-owned fetch, decode, lowering, data-only gadget dispatch, register state, memory access, and structured exits under `arch/orlix`. Instruction subsets trimmed to a package workload, exact-opcode production special cases, host-native guest execution, and silent semantic approximations are not valid completion strategies.

TCTI is not a second kernel, a Linux syscall emulator, or a general Orlix runtime. OrlixKernel remains Linux and owns VMAs, page tables, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics. TCTI exists only at the guest AArch64 EL0 execution boundary required by iOS executable-memory restrictions, as established by [ADR 0022](../architecture-decision/0022-use-hosted-linux-elf-execution.md).

KUnit owns instruction decoding, state-transition, reserved-encoding, and structured-exit proof. Linux kselftest owns Linux-visible ISA integration. The authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
