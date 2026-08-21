---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-08-21
status: active
summary: "Private Orlix-owned AArch64 EL0 execution component for safe Linux ELF execution."
part_of:
  - "[Orlix](../product/orlix.md)"
derived_from:
  - "[TCTI reference review](../../sources/tcti/reference-review.md)"
---

# OrlixTCTI

`OrlixTCTI` is the component name. It is private execution machinery under `arch/orlix/hosted_exec/orlix_tcti`, not a public SDK, product, or compatibility alias.

The authoritative current Linux runtime capability projection is declared by `arch/orlix/include/asm/isa.h`: Armv8.0-A with floating point, AdvSIMD, AES, polynomial multiply, SHA-1, SHA-2, CRC32, SHA-3, SM3, SM4, and SHA-512, exposed to Linux userspace through their corresponding `HWCAP` bits with no `HWCAP2` features. This declaration must match the userspace compiler target. It controls only what Linux may advertise at the current implementation checkpoint.

The complete TCTI target is independent of that runtime projection. It requires classification of all 4,350 leaves in the pinned Arm source, preservation of the union of applicable EL0 feature configurations, and retention of unadvertised extension variants as blocking work. An optional extension may be advertised only after its complete EL0 instruction families, legal encoding boundaries, architectural semantics, deterministic exception behavior, production-path KUnit, and applicable Linux-visible kselftest are complete. Until then, runtime rejection of an unadvertised extension keeps Linux safe, but it does not classify, implement, or prove the extension's target leaves.

`arch/orlix/hosted_exec/orlix_tcti/isa_coverage.h` owns the machine-readable instruction-family inventory. Coverage status is independent of decoder presence: a row remains partial until direct KUnit evidence covers production-path boundaries, state transitions, and exceptions. KUnit ratchets the remaining gap count to zero without treating planned test names as evidence.

The A64 `HLT` encoding is inventoried explicitly. Orlix does not expose external halting debug or semihosting, so TCTI reports `HLT` through structured unsupported-instruction state without advancing the instruction PC and Linux delivers `SIGILL`; `BRK` remains a structured breakpoint delivered as `SIGTRAP`.

TCTI's completion target is complete ISA-on-ISA execution for every applicable architecturally valid AArch64 EL0 instruction in that complete target, with direct classification and required EL0 rejection proof for every non-executable leaf. The production path uses Orlix-owned fetch, decode, lowering, data-only gadget dispatch, register state, memory access, and structured exits under `arch/orlix/hosted_exec/orlix_tcti`. Instruction subsets trimmed to a package workload, exact-opcode production special cases, host-native guest execution, and silent semantic approximations are not valid completion strategies.

Executable blocks carry the stable per-mm mapping generation that authorized their construction. Fetch, cached translation lookup, and guest memory execution must revalidate that generation and hold its mapping-access authorization; a PTE mutation makes stale authorization unusable even when the replacement resolves to the same address or page frame.

Production TCTI behavior, the ISA inventory, diagnostics, and correctness proof are C-native under `arch/orlix/hosted_exec/orlix_tcti`. External-language code may not generate TCTI behavior, define the inventory, model architectural semantics, act as an instruction oracle, or replace the owning KUnit and kselftest proof.

TCTI is not a second kernel, a Linux syscall emulator, or a general Orlix runtime. OrlixKernel remains Linux and owns VMAs, page tables, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics. TCTI exists only at the guest AArch64 EL0 execution boundary required by iOS executable-memory restrictions, as established by [ADR 0022](../architecture-decision/0022-use-hosted-linux-elf-execution.md).

KUnit owns instruction decoding, state-transition, reserved-encoding, and structured-exit proof. Linux kselftest owns Linux-visible ISA integration. The authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).

Base A64 load, store, and prefetch proof lives in the `BASE_LOAD_STORE` cohort. AdvSIMD structure load and store proof lives in the `ADVSIMD_LOAD_STORE` cohort. Exclusive, LSE, ordered, RCW, LS64, and related atomic proof lives in the `BASE_ATOMICS` cohort of 498 leaves. Base A64 add and subtract proof lives in the `BASE_ADD_SUBTRACT` cohort of 34 leaves. Base A64 branch and control-flow proof lives in the `BASE_CONTROL_FLOW` cohort of 10 leaves. Base A64 exceptions and architecturally undefined proof lives in the `BASE_EXCEPTIONS` cohort of 10 leaves. Base A64 conditional compare, select, and branch proof lives in the `BASE_CONDITIONAL` cohort of 60 leaves. Base A64 bitfield, extract, and unary integer proof lives in the `BASE_BITFIELD_UNARY` cohort of 25 leaves. Shared guest-memory fixtures live in `orlix_tcti_memory_proof.h`. A production capture session is selected by source ordinal and obligation. Memory-family success walks `REGISTERS`, `MEMORY`, and `PC`, plus `ATOMICITY` and `ORDERING` where the leaf requires them. Add and subtract success walks `REGISTERS`, `PC`, and `FLAGS`. Direct and register EL0 branch success walks `REGISTERS` and `PC`. `FLAGS`, `MEMORY`, `ATOMICITY`, and `ORDERING` are not applicable for those EL0 branch leaves. ERET, ERETAA, ERETAB, DRPS, and TEXIT are `NON_EL0` and prove EL0 rejection. TEXIT official DDI0602 semantics are unspecified. SVC has production `REGISTERS` and `PC` observations on the structured syscall exit. BRK, HLT, and UDF prove EL0 structured exits. HVC, SMC, DCPS1, DCPS2, DCPS3, and TENTER prove EL0 rejection. TENTER official DDI0602 semantics are unspecified. The 23 always-on EL0 `BASE_CONDITIONAL` leaves `B.cond`, `CBZ`/`CBNZ`, `TBZ`/`TBNZ`, `CSEL`/`CSINC`/`CSINV`/`CSNEG`, and `CCMP`/`CCMN` have production `REGISTERS` and `PC` observations, plus `FLAGS` for the compare forms. `BC.cond` and the 36 `FEAT_CMPBR` compare-and-branch leaves are `NON_EL0` and prove EL0 rejection. `FEAT_CMPBR` and `FEAT_HBC` are not advertised. The 19 always-on EL0 `BASE_BITFIELD_UNARY` leaves `EXTR`, `SBFM`/`BFM`/`UBFM`, and `RBIT`/`REV*`/`CLZ`/`CLS` have production `REGISTERS` and `PC` observations. `CTZ`/`CNT`/`ABS` (`FEAT_CSSC`) are `NON_EL0` and prove EL0 rejection. `FEAT_CSSC` is not advertised. `MEMORY`, `ATOMICITY`, and `ORDERING` are not applicable for add and subtract. A matching user fault may credit only a `FAULTS` observation through engine evidence. A successful gadget does not credit `FAULTS`. Pair RCW is UNDEFINED in the product scalar RCW domain. STL1 and LDAP1 are transfer-only in the AdvSIMD structure cohort. COW and self-modifying-code invalidation remain in the mapping-invalidation suite. ADDPT and SUBPT keep PAC tag bits [63:56] and do 56-bit pointer arithmetic. `HWCAP_ATOMICS` is not advertised. `FEAT_CPA` is not advertised. `FEAT_TEV` is not advertised.
