---
type: meta
tags:
  - documentation
  - history
updated: 2026-08-25
---
# Orlix Knowledge Log

## [2026-08-25] record | Prove ADVSIMD_INTEGER lane-width saturation and sticky QC

[CORRECTION] Unsigned saturating add overflow is compared against the element mask, not 64-bit wrap. Signed saturating add and subtract use a 128-bit sum so a 64-bit lane cannot wrap before saturate. USQADD saturates against the unsigned lane bounds. Doubling multiplies use a 128-bit product. SRI with shift equal to the element width keeps the destination. ABS and NEG negate in unsigned arithmetic. SSHL left-shifts in unsigned arithmetic. SQSHLU saturates a negative source to 0 even at `#0`. Credited captures run from both clear and set QC, seed preserved FPSR exception bits and a nonzero FPCR, and compare the complete FPSR and FPCR. The SIMD seed uses mixed-sign lanes and a `-1` variable-shift count. Legal Q and size variants are compared before ingest. The size walk skips reserved encodings that a wide source mask still binds, including CNT/NOT size!=00, RBIT size!=01, and CLS/CLZ/REV64 size=11. Immediate-shift leaves walk legal immh values that still match the source mask, including non-one-hot encodings such as 16-bit `immh=3`, and every `immb` 0..7. Scalar asisdshf SSHR/SHL/SLI/SRI bind 64-bit only. By-element leaves walk legal H/L indexes and, for 32-bit and 64-bit, Rm bit 20. Ordinary leaves compare one V16-V31 encoding for each free Rd, Rn, and Rm field, plus Rd==Rn and Rd==Rm when those fields are free. Doubling-multiply leaves capture signed-minimum operands that saturate and set QC. Modified-immediate leaves walk every `cmode` the source mask permits, plus imm8 0x00, 0x5A, and 0xFF. Q=1 2D miscellaneous ABS/NEG/SQABS/SQNEG compare before ingest. Credited captures assert GPRs and PSTATE are unchanged. SQDMLAL/SQDMLSL saturate the doubled product, then saturate the accumulate. ADD `Rd==Rn` and `Rd==Rm` compare the full expected vector. Resume and preserved-SIMD assertions abort the capture function before ingest. `linux_proof_executed` stays 0.

## [2026-08-23] record | Verify ADVSIMD_INTEGER SIMD results and keep reserved encodings unsupported

Recorded that reserved AdvSIMD integer encodings the raw decoder left unsupported, including ADD `Q=0,size=3` (`0x0ee08400`) and scalar `asisdshf` SSHR with `immh == 0000` (`0x5f000400`), stay `DECODE_UNSUPPORTED` and do not promote from a source-manifest mask. Vector `0x0f000400` is legal `MOVI`, not reserved SSHR. Production EL0 proof for all 222 always-on leaves compares destination SIMD and QC against a source-mnemonic architectural result before ingest. Saturating register shifts that exceed the element width set FPSR.QC. `linux_proof_executed` stays 0.

## [2026-08-23] record | Keep optional ADVSIMD_FP leaves in the EL0 complete target

[CORRECTION] The following `ADVSIMD_FP` land entry is wrong to classify all 268 leaves and to call the 155 optional leaves `NON_EL0`. Optional FP16, FHM, BF16, FCMA, FRINTTS, FP8, FAMINMAX, and FSCALE AdvSIMD leaves remain architecturally valid EL0 when their features apply. Runtime rejection without `HWCAP_FPHP` / `HWCAP_ASIMDHP` / related bits does not classify those 155 leaves `NON_EL0` and does not complete them. They stay unclassified blockers in the complete target. Unimplemented FP16 forms and reserved `immh == 0000` encodings stay `DECODE_UNSUPPORTED`. Production EL0 proof compares destination SIMD, FPSR, resume success, and every non-destination SIMD register against a source-mnemonic architectural result before ingest, including every legal size and Q variant plus pinned infinity, signed-zero, subnormal, signaling-NaN, and non-default FPCR rounding vectors. The size and Q walk skips reserved encodings that production rejects: vector three-same `Q=0` 2D, and by-element 2D with `L=1`. For 2D by-element it clears `L` so the index is `H` only. `linux_proof_executed` stays 0.

## [2026-08-23] record | Land the ADVSIMD_FP proof contract

Recorded that the 268 unique `ADVSIMD_FP` leaves have production decode and EL0 classification. The 113 always-on EL0 leaves have production observations for `FP_SIMD` and `PC`. `MEMORY`, `ATOMICITY`, `ORDERING`, and `FLAGS` are not applicable. NaN, infinity, signed-zero, subnormal, FPCR rounding, FPSR IOC, Q=0 upper-half clearing, reserved Q=0 2D, and overlapping source/destination registers are proved. The 155 optional FP16, FHM, BF16, FCMA, FRINTTS, FP8, FAMINMAX, and FSCALE leaves are classified `NON_EL0` and prove EL0 rejection. `HWCAP_FPHP`, `HWCAP_ASIMDHP`, `HWCAP_ASIMDFHM`, `HWCAP_BF16`, FCMA, FRINTTS, FAMINMAX, FP8, and FSCALE are not advertised. `linux_proof_executed` stays 0. The 4,350-leaf coverage task stays doing.

## [2026-08-23] record | Land the ADVSIMD_INTEGER proof contract

Recorded that the 240 unique `ADVSIMD_INTEGER` leaves have production decode and EL0 classification. The 222 always-on EL0 leaves have production observations for `FP_SIMD` and `PC`. `MEMORY`, `ATOMICITY`, `ORDERING`, and `FLAGS` are not applicable. Lane endpoints, signed versus unsigned saturation, QC updates, high-half forms, narrowing destination preservation, variable shift counts outside element width, and overlapping source/destination registers are proved. The 18 optional RDM/DotProd/I8MM leaves are classified `NON_EL0` and prove EL0 rejection. XTN and PMUL stay in this family. `HWCAP_ASIMDRDM`, `HWCAP_ASIMDDP`, and `HWCAP_I8MM` are not advertised. `linux_proof_executed` stays 0. The 4,350-leaf coverage task stays doing.

## [2026-08-22] record | Land the ADVSIMD_PERMUTE_MOVE proof contract

Recorded that the 37 unique `ADVSIMD_PERMUTE_MOVE` leaves have production decode and EL0 classification. The 33 always-on EL0 leaves DUP/INS/UMOV/SMOV, ZIP/UZP/TRN/EXT/REV, TBL/TBX, and MOVI have production observations for `FP_SIMD` and `PC`, plus `REGISTERS` for UMOV/SMOV. `MEMORY`, `ATOMICITY`, `ORDERING`, and `FLAGS` are not applicable. LUTI2/LUTI4 (`FEAT_LUT`) are classified `NON_EL0` and prove EL0 rejection. `FEAT_LUT` is not advertised. `linux_proof_executed` stays 0. The 4,350-leaf coverage task stays doing.

## [2026-08-22] record | Land the ADVSIMD_CRYPTO proof contract

Recorded that the 32 unique `ADVSIMD_CRYPTO` leaves have production decode and EL0 classification. All 32 EL0 leaves AESE/AESD/AESMC/AESIMC, SHA-1/SHA-256/SHA-512, SHA-3 (EOR3/BCAX/RAX1/XAR), SM3, SM4, and PMULL have production observations for `FP_SIMD` and `PC`. `MEMORY`, `ATOMICITY`, `ORDERING`, and `FLAGS` are not applicable. Destructive destinations, source/destination aliasing, PMULL versus PMULL2 source halves, all-zero/all-one polynomial operands, AES round ordering, and Q=0 upper-bit preservation are proved. `HWCAP_AES`, `HWCAP_PMULL`, `HWCAP_SHA1`, `HWCAP_SHA2`, `HWCAP_SHA3`, `HWCAP_SHA512`, `HWCAP_SM3`, and `HWCAP_SM4` are not advertised. `linux_proof_executed` stays 0. The 4,350-leaf coverage task stays doing.

## [2026-08-22] record | Land the BASE_MULTIPLY_DIVIDE proof contract

Recorded that the 16 unique `BASE_MULTIPLY_DIVIDE` leaves have production decode and EL0 classification. All 16 EL0 leaves `UDIV`/`SDIV`, `MADD`/`MSUB`, `SMADDL`/`SMSUBL`/`SMULH`, `UMADDL`/`UMSUBL`/`UMULH`, and `MADDPT`/`MSUBPT` have production observations for `REGISTERS` and `PC`. `MEMORY`, `ATOMICITY`, `ORDERING`, and `FLAGS` are not applicable. Zero divisor, signed minimum divided by `-1`, W upper-zeroing, long-form source extension, high-half products, accumulator aliasing, and XZR operands are proved. MADDPT and MSUBPT keep PAC tag bits [63:56] and do 56-bit pointer arithmetic. `FEAT_CPA` is not advertised. `linux_proof_executed` stays 0. The 4,350-leaf coverage task stays doing.

## [2026-08-21] record | Land the BASE_BITFIELD_UNARY proof contract

Recorded that the 25 unique `BASE_BITFIELD_UNARY` leaves have production decode and EL0 classification. The 19 always-on EL0 leaves `EXTR`, `SBFM`/`BFM`/`UBFM`, and `RBIT`/`REV*`/`CLZ`/`CLS` have production observations for `REGISTERS` and `PC`. `MEMORY`, `ATOMICITY`, `ORDERING`, and `FLAGS` are not applicable. `CTZ`/`CNT`/`ABS` (`FEAT_CSSC`) are classified `NON_EL0` and prove EL0 rejection. `FEAT_CSSC` is not advertised. `linux_proof_executed` stays 0. The 4,350-leaf coverage task stays doing.

## [2026-08-21] record | Bind every EL0 BASE_CONDITIONAL #120 capture

Recorded that the `BASE_CONDITIONAL` production capture family now binds all 23 always-on EL0 ordinals. Each leaf has typed `REGISTERS` and `PC` observations. The eight `CCMP`/`CCMN` leaves also have `FLAGS`. The earlier five-ordinal representative list is gone. `linux_proof_executed` stays 0. `FEAT_CMPBR` and `FEAT_HBC` are not advertised. The 4,350-leaf coverage task stays doing.

## [2026-08-21] record | Land the BASE_CONDITIONAL proof contract

Recorded that the 60 unique `BASE_CONDITIONAL` leaves have production decode and EL0 classification. The 23 always-on EL0 leaves `B.cond`, `CBZ`/`CBNZ`, `TBZ`/`TBNZ`, `CSEL`/`CSINC`/`CSINV`/`CSNEG`, and `CCMP`/`CCMN` have production observations for `REGISTERS` and `PC`, plus `FLAGS` for the compare forms. `MEMORY`, `ATOMICITY`, and `ORDERING` are not applicable. `BC.cond` (`FEAT_HBC`) and the 36 `FEAT_CMPBR` compare-and-branch leaves are classified `NON_EL0` and prove EL0 rejection. `FEAT_CMPBR` and `FEAT_HBC` are not advertised. `linux_proof_executed` stays 0. The 4,350-leaf coverage task stays doing.

## [2026-08-21] record | Land the BASE_EXCEPTIONS proof contract

Recorded that the 10 unique `BASE_EXCEPTIONS` leaves have production decode and EL0 classification. SVC has production observations for `REGISTERS` and `PC` through the structured `EXIT_SYSCALL` path. BRK, HLT, and UDF prove architecturally required EL0 structured exits. HVC, SMC, DCPS1, DCPS2, DCPS3, and TENTER are classified `NON_EL0` or architecturally undefined and prove EL0 rejection. TENTER official DDI0602 semantics are unspecified. Linux keeps syscall dispatch. `linux_proof_executed` stays 0. `FEAT_TEV` is not advertised. The 4,350-leaf coverage task stays doing.

## [2026-08-20] record | Land the BASE_CONTROL_FLOW proof contract

Recorded that the 10 unique `BASE_CONTROL_FLOW` leaves have production decode and EL0 classification. The five EL0 leaves B, BL, BR, BLR, and RET have production observations for `REGISTERS` and `PC`. `FLAGS`, `MEMORY`, `ATOMICITY`, and `ORDERING` are not applicable for those EL0 leaves. ERET, ERETAA, ERETAB, DRPS, and TEXIT are classified `NON_EL0` and prove EL0 rejection. TEXIT official DDI0602 semantics are unspecified. Reserved encodings stay decode `UNSUPPORTED`. `linux_proof_executed` stays 0. `FEAT_TEV` is not advertised. The 4,350-leaf coverage task stays doing.

## [2026-08-20] record | Land the BASE_ADD_SUBTRACT proof contract

Recorded that the 34 unique `BASE_ADD_SUBTRACT` leaves have production decode, execute, and typed native observations for `REGISTERS`, `PC`, and `FLAGS`. `MEMORY`, `ATOMICITY`, and `ORDERING` are not applicable. Immediate, shifted, extended, and carry forms stay on one production-capture suite with ADDPT and SUBPT. ADDPT and SUBPT keep PAC tag bits [63:56] and do 56-bit pointer arithmetic. Reserved encodings stay decode `UNSUPPORTED`. `linux_proof_executed` stays 0. `FEAT_CPA` is not advertised. The 4,350-leaf coverage task stays doing.

## [2026-08-20] record | Land the BASE_ATOMICS proof contract

Recorded that the 498 unique `BASE_ATOMICS` leaves have production decode, execute, and typed native observations for `REGISTERS`, `MEMORY`, and `PC` on success, plus `ATOMICITY` and `ORDERING` where the leaf requires them, and `FAULTS` on the unmapped walk. Exclusive-monitor mismatch, CAS/RMW old-value, and CASP whole-pair extras (including 32-bit pair packing) stay on the same suite. Product `FEAT_THE` with `FEAT_D128` disabled keeps pair RCW UNDEFINED at EL0. `linux_proof_executed` stays 0. `HWCAP_ATOMICS` is not advertised. The 4,350-leaf coverage task stays doing.

## [2026-08-18] record | Land the ADVSIMD_LOAD_STORE proof contract

Recorded that the 152 unique `ADVSIMD_LOAD_STORE` leaves have production decode, execute, and typed native observations for `REGISTERS`, `MEMORY`, `PC`, and `FAULTS`. The family uses the same `orlix_tcti_memory_proof.h` fixtures and ordinal-plus-obligation capture walk as `BASE_LOAD_STORE`. STL1 and LDAP1 are proved for structure transfer only. Their release/acquire ordering is not proved. Exclusive, LSE, SVE, and SME memory families remain open. The 4,350-leaf coverage task stays doing.

## [2026-08-18] record | Land the BASE_LOAD_STORE proof contract

Recorded that the 209 unique `BASE_LOAD_STORE` leaves have production decode, execute, and typed native observations for `REGISTERS`, `MEMORY`, `PC`, and `FAULTS`. Later memory families reuse `orlix_tcti_memory_proof.h` and the ordinal-plus-obligation capture walk. A matching user fault credits only a `FAULTS` observation. Exclusive, LSE, AdvSIMD-structure, SVE, and SME memory families remain open. The 4,350-leaf coverage task stays doing.

## [2026-07-23] align | Separate full ISA proof from runtime promotion

Aligned the TCTI component, coverage task, simulator ladder, default-promotion task, and roadmap with ADRs 0017, 0022, and 0029. The 4,350-leaf Arm inventory remains the completion target, LSE requires production-path KUnit and Linux-visible kselftest before `HWCAP_ATOMICS`, the mlibc-linked syscall/UAPI rerun remains a distinct proof stage, and external-language generators or semantic oracles cannot define TCTI correctness.

## [2026-07-23] correct | Make the complete Arm source inventory the TCTI goal

Added ADR 0029 and corrected the active TCTI epic, story, tasks, and roadmap so
the complete pinned 4,350-leaf Arm inventory defines
ISA-on-ISA completion. Every leaf now requires an explicit target
classification, applicable EL0 feature alternatives remain blocking across the
union of feature configurations, and privileged leaves require their
architecturally correct EL0 behavior. The current Linux HWCAP and HWCAP2 profile
is a separate safe-advertisement projection and cannot hide missing instruction
semantics or make the completeness audit pass.

## [2026-07-23] decide | Keep TCTI implementation and proof kernel-native

Amended ADR 0022 so TCTI implementation, ISA inventory, structured diagnostics,
and correctness proof remain C-native under `arch/orlix`, with KUnit and
kselftest as the authoritative owning surfaces. External-language scripts may
not generate or model TCTI behavior, define its ISA inventory, or replace
structured kernel proof.

## [2026-07-22] implement | Expand configured AArch64 ISA coverage

Expanded the guest profile and direct production-path proof for the configured
Armv8.0-A floating-point, AdvSIMD, AES, polynomial-multiply, SHA-1, SHA-2,
CRC32, SHA-3, SM3, SM4, and SHA-512 surface. The kernel-owned inventory reports
56 of 56 instruction families complete with zero ratcheted gaps. KUnit now
covers 446 cases with zero failures or skips, including exhaustive SVC, BRK,
and HLT immediate decoding and structured HLT state. Linux kselftest proves
BRK reaches `SIGTRAP` while HLT reaches `SIGILL`, and the app-hosted run reaches
`ORLIX-KSELFTEST-END` before its XCTest reports one passed test with zero
failures. The owning `xcodebuild` command still requires interruption after the
native pass because test-session cleanup does not terminate, so clean gate
termination and the broader ISA-completeness objective remain open. LLVM
AArch64 TableGen 22.1.8 and OpenMinis ish-arm64 commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` were independent inventory and
encoding cross-checks. No external implementation source was copied.
The TCTI component catalog summary now identifies its AArch64 EL0 ownership.

## [2026-07-21] implement | Complete AdvSIMD vector three-different coverage

Closed the configured Armv8.0-A AdvSIMD vector three-different inventory row
against LLVM AArch64 TableGen commit
`de44ed3488693686dd1f65baad5d7bd3797eddca`. Existing production decoder,
executor, and direct KUnit evidence already covered narrowing add/subtract,
signed and unsigned absolute-difference long and accumulate, widening
add/subtract, wide add/subtract, signed and unsigned multiply-long accumulate
and subtract, and polynomial multiply. Added the missing vector and upper-half
`SQDMULL`, `SQDMLAL`, and `SQDMLSL` forms through the existing Orlix-owned
decoder and saturating execution helper. New KUnit evidence covers both legal
source widths, both source halves, every register field, source-destination and
all-register aliases, reserved widths, ordinary and saturating results, sticky
`FPSR.QC`, full-vector writes, unrelated SIMD and integer state, and exact PC
progression. The app-hosted gate reports 407 of 407 KUnit cases passing, reaches
`ORLIX-KSELFTEST-END`, and passes its single XCTest in 12.166 seconds with zero
failures. The owning command returned exit zero after terminating only Xcode's
lingering `simctl diagnose` cleanup child. The structured result bundle reports
one passed test, zero failures, and zero skipped tests. OpenMinis ish-arm64 `math.S` and
`gen.c` at commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied an
independent family-decomposition cross-check. No external implementation source
was copied. The kernel-owned inventory now reports 49 of 52 families complete
with three explicit gaps.

## [2026-07-21] audit | Complete AdvSIMD vector three-same coverage

Closed the configured Armv8.0-A AdvSIMD vector three-same inventory row after
auditing the production decoder, executor, and direct KUnit evidence against
the 77 required integer, floating-point, compare, logical, saturating, shift,
multiply, polynomial, pairwise, and accumulator operations in LLVM AArch64
TableGen commit `de44ed3488693686dd1f65baad5d7bd3797eddca`. Every required
operation was already implemented through the Orlix-owned production path; the
coverage row had remained partial pending the whole-family audit, so no
duplicate production operation or alternate decoder was added. Existing KUnit
evidence covers legal vector shapes, every register field, source and
destination aliases, 64-bit upper-half clearing, saturation and sticky QC,
FPCR and FPSR behavior, NaNs and signed zero, exact PC progression, unrelated
state preservation, and reserved shape rejection. RDM-only `SQRDMLAH` and
`SQRDMLSH` remain outside the advertised guest profile because `ELF_HWCAP2` is
zero. The app-hosted gate reports 405 of 405 KUnit cases passing, reaches
`ORLIX-KSELFTEST-END`, and passes its single XCTest in 7.984 seconds with zero
failures; the owning command exits zero after terminating only the separately
tracked `simctl diagnose` cleanup child. OpenMinis ish-arm64 `math.S` and
`gen.c` at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied an independent family and
gadget-decomposition cross-check; no external implementation source was
copied. The kernel-owned inventory now reports 48 of 52 families complete with
four explicit gaps.

## [2026-07-21] implement | Complete baseline AdvSIMD scalar coverage

Closed the configured Armv8.0-A AdvSIMD scalar data-processing inventory row.
The production decoder and ISA-on-ISA executor now cover scalar `FRECPE`,
`FRECPX`, `FRSQRTE`, and `FCVTXN`, completing the operations missing from the
already implemented scalar arithmetic, comparison, shift, conversion,
saturating, narrowing, pairwise, and element-transfer subfamilies. The new
native execution path preserves host FPCR and FPSR, installs and returns guest
FP state, clears the architectural upper destination bits, and preserves
unrelated SIMD, integer, SP, PSTATE, and PC state. KUnit enumerates every source
and destination register encoding for all legal single-precision,
double-precision, and narrowing forms, rejects disabled FP16 and malformed
`FCVTXN` shapes, and executes every register alias combination plus zero,
infinity, signaling NaN, default-NaN, invalid-operation, divide-by-zero, and
inexact round-to-odd cases. The app-hosted gate reports 405 of 405 KUnit cases
passing, including the three new scalar tests, reaches `ORLIX-KSELFTEST-END`,
and passes its single XCTest in 8.517 seconds with zero failures. The owning
command exited zero after terminating only the separately tracked `simctl
diagnose` cleanup child. LLVM AArch64 TableGen at commit
`de44ed3488693686dd1f65baad5d7bd3797eddca` remains the primary encoding and
family inventory source. OpenMinis ish-arm64 at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` remains an independent family and
gadget-decomposition cross-check. No external implementation source was copied.
The kernel-owned inventory now reports 47 of 52 families complete with five
explicit gaps.

## [2026-07-21] implement | Complete EL0 system, hint, and barrier coverage

Closed the configured Armv8.0-A EL0 system-and-hint inventory row. The
production decoder and executor now cover all 128 `HINT` immediates, all
`CLREX` immediates, legal `DMB`, `DSB`, and `ISB` options, `IC IVAU`, `DC CVAC`,
`DC CVAU`, `DC CIVAC`, and the guest-exposed thread, flag, cache-description,
and virtual-counter registers. Read-only registers reject writes, FPCR and FPSR
mask reserved bits, cache maintenance invalidates translated instruction state
where required, and `YIELD`, `WFE`, and `WFI` produce structured exits that the
Linux execution loop turns into a scheduling point before resuming at the
following instruction. KUnit
exhaustively proves decoder boundaries, register fields, direct state
transitions, counter behavior, and structured yield results, reporting 402 of
402 tests passing. The OrlixMLibC-built `tcti_system_probe` independently proves
the system registers, FP status masks, barriers, cache operations, `CLREX`, and
all three yielding hints through real Linux EL0 execution, with all nine
assertions passing and the complete kselftest run reaching
`ORLIX-KSELFTEST-END`. The owning XCTest passed its single test and
`xcodebuild` exited zero. LLVM AArch64 TableGen at commit
`de44ed3488693686dd1f65baad5d7bd3797eddca` remains the primary encoding source.
OpenMinis ish-arm64 `gen.c` and the AArch64 gadget inventory at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied an independent system
instruction and barrier decomposition cross-check. No external implementation
source was copied. The kernel-owned inventory now reports 46 of 52 families
complete with 6 explicit gaps.

## [2026-07-21] implement | Complete AdvSIMD structure load and store coverage

Closed the configured Armv8.0-A AdvSIMD structure load/store inventory row.
The production decoder now rejects the reserved 64-bit single-structure form
with `S=1`, while retaining the complete `LD1` through `LD4`, `ST1` through
`ST4`, and `LD1R` through `LD4R` single and multiple structure families.
KUnit exhaustively covers all 49,152 single and multiple structure control
combinations and every base and vector-register field. Its execution matrix
proves every legal lane and replicate form, every multiple-structure opcode,
all element widths, both 64-bit and 128-bit vector widths, loads and stores,
sequential and interleaved layouts, register-list wrapping, writeback, memory
fault order, unrelated SIMD state preservation, and exact PC progression. The
app-hosted KUnit and Linux kselftest gate reports 396 of 396 tests passing, and
XCTest passed its single owning test in 8.749 seconds. LLVM AArch64 TableGen at
commit `de44ed3488693686dd1f65baad5d7bd3797eddca` remains the primary encoding
source. OpenMinis ish-arm64 `gen.c` and `gadgets-aarch64/memory.S` at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied an independent structure
count, lane decomposition, interleaving, and writeback cross-check. No external
implementation source was copied. The kernel-owned inventory now reports 45 of
52 families complete with 7 explicit gaps.

## [2026-07-21] implement | Complete exclusive load and store coverage

Closed the configured Armv8.0-A exclusive load/store inventory row. The
production decoder now covers the complete single and pair exclusive family,
including acquire and release forms, while rejecting later ordered
nonexclusive encodings outside the configured profile and reserved field
combinations. The executor records the loaded reservation value, performs
store-exclusive through an atomic compare-exchange against guest memory,
detects intervening writes, supports 8-byte and 16-byte pair reservations,
enforces architectural alignment, and clears the local monitor after attempts,
ordinary TCTI stores, `CLREX`, and task switches. KUnit exhaustively covers
every exclusive control shape and register field, then proves `LDXR`/`STXR`,
`LDAXR`/`STLXR`, `LDXP`/`STXP`, `LDAXP`/`STLXP`, interference failure, status
results, memory effects, alignment faults, monitor state, and exact PC
behavior. LLVM AArch64 TableGen at commit
`de44ed3488693686dd1f65baad5d7bd3797eddca` remains the primary encoding
source. OpenMinis ish-arm64 `gen.c` and `gadgets-aarch64/memory.S` at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied an independent
pair-decomposition and compare-exchange cross-check. No external implementation
source was copied. The kernel-owned inventory now reports 44 of 52 families
complete with 8 explicit gaps.

## [2026-07-21] implement | Complete single-register load and store coverage

Closed the configured Armv8.0-A single-register load and store inventory row.
The production decoder now accepts unprivileged `LDTR` and `STTR` integer
variants, every unsigned-immediate SIMD B/H/S/D/Q transfer, and architectural
`PRFM` and `PRFUM` hints while preserving reserved prefetch, SIMD
unprivileged, and invalid extend-option boundaries. KUnit exhaustively covers
all unsigned and signed immediate control shapes and immediate values, every
register-offset control shape and offset register, every base and transfer
register field including register 31 and aliases, plus structured execution
for sign extension, memory writes, SIMD B/H transfers, prefetch hints,
unprivileged no-writeback behavior, exact PC progression, and memory faults.
LLVM AArch64 TableGen at commit
`de44ed3488693686dd1f65baad5d7bd3797eddca` remains the primary encoding
source. OpenMinis ish-arm64 `gen.c` and `gadgets-aarch64/memory.S` at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied an independent addressing,
width, extension, and gadget-decomposition cross-check. No external
implementation source was copied. The kernel-owned inventory now reports 43 of
52 families complete with 9 explicit gaps.

## [2026-07-21] verify | Close SHA1 and SHA256 ISA coverage

Closed the configured Armv8.0-A SHA1 and SHA256 inventory row without replacing
the existing Orlix decoder or execution algorithms. New KUnit coverage enumerates
all 232,448 legal register encodings for SHA1C/P/M/H/SU0/SU1 and
SHA256H/H2/SU0/SU1, including register aliases, and rejects all 32,768 reserved
three-register SHA256 operation-3 encodings. Existing structured execution tests
continue to prove every operation's state transition. LLVM AArch64 TableGen at
commit `de44ed3488693686dd1f65baad5d7bd3797eddca` remains the primary encoding
source. OpenMinis ish-arm64 `crypto_helpers.c`, `crypto.S`, and `gadgets.h` at
commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied independent operation,
lane-layout, and native-gadget cross-checks. No external implementation source
was copied. The kernel-owned coverage inventory now marks `SHA1_SHA256` complete.

## [2026-07-21] implement | Complete floating-point fixed-point conversions

Completed the configured Armv8.0-A floating-point fixed-point conversion family
across GPR, AdvSIMD scalar, and AdvSIMD vector forms. The production decoder and
executor now cover signed and unsigned `FCVTZ` and `CVTF` in both directions for
all W/X with S/D forms and scalar S/D plus vector 2S/4S/2D shapes. Every legal
fractional-bit immediate uses the exact native AArch64 operation while preserving
guest FPCR and FPSR state. KUnit covers 1,703,936 legal decode combinations,
262,144 reserved one-lane double-vector encodings, 131,072 disabled FP16
encodings, 1,664 every-immediate value executions, signed negative results,
unsigned invalid-operation diagnostics, aliases, upper-lane clearing, unrelated
register preservation, and PC progression. LLVM AArch64 TableGen at commit
`de44ed3488693686dd1f65baad5d7bd3797eddca` remains the primary encoding source.
OpenMinis ish-arm64 `gen.c` and `math.S` at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` supplied independent mask and gadget
decomposition cross-checks. No external implementation source was copied. The
kernel-owned coverage inventory now marks `FP_FIXED_POINT_CONVERT` complete.

## [2026-07-21] implement | Complete floating-point integer conversions

Completed the configured Armv8.0-A floating-point integer-conversion family
across GPR, AdvSIMD scalar, and AdvSIMD vector forms. The production decoder now
covers `FCVTNS`, `FCVTNU`, `FCVTPS`, `FCVTPU`, `FCVTMS`, `FCVTMU`, `FCVTZS`,
`FCVTZU`, `FCVTAS`, `FCVTAU`, `SCVTF`, and `UCVTF` for scalar `S` and `D` and
vector `2S`, `4S`, and `2D` shapes. Execution uses the existing Orlix TCTI
register model and native FPCR/FPSR-preserving helper. KUnit covers 61,440 legal
decode combinations, 12,288 reserved one-lane double-vector encodings, 36,864
disabled FP16 encodings, and 60 value-level shape and operation executions with
aliases, signed and unsigned lanes, exact rounding, sticky FPSR state, upper-lane
clearing, unrelated-register preservation, and PC progression. The pinned
app-hosted KUnit and Linux kselftest gate passed one XCTest with zero failures
and zero skips. LLVM AArch64 TableGen at commit
`de44ed3488693686dd1f65baad5d7bd3797eddca` was the primary encoding source.
OpenMinis ish-arm64 `gen.c` and `math.S` at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` were independent mask and gadget
decomposition cross-checks. No external implementation source was copied. The
kernel-owned coverage inventory now marks `FP_INTEGER_CONVERT` complete while
`FP_FIXED_POINT_CONVERT` remains partial.

## [2026-07-21] implement | Extend scalar floating-point integer conversions

Added the baseline A64 scalar floating-point-to-GPR `FCVTNS`, `FCVTNU`,
`FCVTPS`, `FCVTPU`, `FCVTMS`, `FCVTMU`, `FCVTAS`, and `FCVTAU` operations for
single and double sources and 32-bit and 64-bit destinations. The generic
decoder claims only conversion encodings it owns and falls through for existing
transfer and specialized conversion families. Existing `FCVTZS`, `FCVTZU`,
fixed-point, SIMD, `SCVTF`, `UCVTF`, and `FMOV` behavior remains present. KUnit
exhausts every source and destination register field, source and destination
width, rounding mode, and signedness, and executes positive and negative
fractional cases under a conflicting FPCR rounding mode. The app-hosted KUnit
and Linux kselftest gate passed one XCTest with zero failures and zero skips on
the pinned simulator after releasing the separately tracked `simctl diagnose`
cleanup child. The broader FP integer and fixed-point conversion inventory rows
remain partial.

## [2026-07-21] implement | Complete scalar floating-point one-source execution

Completed the configured Armv8.0-A scalar floating-point one-source family by adding both single/double `FCVT` directions and `FSQRT` to the exact native FPCR/FPSR-aware execution path while retaining the established `FMOV`, `FABS`, `FNEG`, and seven `FRINT` operations. Replaced the manual widening conversion helper with architectural AArch64 execution, added exhaustive legal and reserved encoding boundaries, all register fields and aliases, narrowing rounding-mode and inexact behavior, signaling NaN default-NaN behavior, square-root invalid-operation behavior, upper-bit clearing, unrelated state preservation, and exact PC progression. The new exhaustive scalar move proof exposed and fixed a pre-existing `FMOV S,S` bug that preserved bits 63:32 instead of truncating the 32-bit result. The kernel-owned coverage inventory now marks `FP_1SOURCE` complete and reduces its ratcheted gap count from 14 to 13. The app-hosted KUnit and Linux kselftest gate reached `ORLIX-KSELFTEST-END`; XCTest passed one test with zero failures and zero skips, and `xcodebuild` exited zero after its separately tracked `simctl diagnose` cleanup child was released. Patch-only Linux `checkpatch.pl` reported zero errors and zero warnings. LLVM AArch64 TableGen at commit `de44ed3488693686dd1f65baad5d7bd3797eddca` and OpenMinis ish-arm64 at commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` were independent family and encoding cross-checks. No external implementation source was copied.

## [2026-07-21] implement | Complete baseline AdvSIMD vector floating-point three-same execution

Added baseline A64 vector `FADDP`, `FADD`, `FDIV`, `FMAXNMP`, `FMAXNM`, `FMAXP`, `FMAX`, `FMINNMP`, `FMINNM`, `FMINP`, `FMIN`, `FMLA`, `FMLS`, `FMUL`, and `FSUB` decode and exact native execution without removing existing instruction families or replacing the established `FMUL` 2D path. Production execution preserves guest and host FPCR/FPSR state, uses the original destination as the architectural accumulator for fused multiply-add and multiply-subtract, and commits results through the normal TCTI SIMD register path. KUnit covers every operation across legal 2S, 4S, and 2D shapes, destination and source aliases, upper-half clearing, unrelated SIMD and integer state, exact PC progression, numeric versus propagating NaNs, signaling NaNs with default-NaN mode, signed zero, divide-by-zero and invalid flags, pairwise lane ordering, and a fused-only `FMLA` result. The app-hosted KUnit and Linux kselftest gate reached `ORLIX-KSELFTEST-END`; XCTest passed one test with zero failures and zero skips, and `xcodebuild` exited zero after its separately tracked `simctl diagnose` cleanup child was released. Patch-only Linux `checkpatch.pl` reported zero errors and zero warnings. LLVM AArch64 encodings and OpenMinis ish-arm64 `math.S` at commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` were independent encoding and family-decomposition cross-checks. No external implementation source was copied.

## [2026-07-21] implement | Add scalar AdvSIMD floating-point pairwise execution

Added baseline A64 scalar `FADDP`, `FMAXNMP`, `FMAXP`, `FMINNMP`, and
`FMINP` decode and execution without removing existing instruction families or
prematurely promoting the broader `ASIMD_SCALAR` inventory row. The production
path preserves host FPCR and FPSR, installs guest floating-point state with
preemption disabled, executes the exact single-precision or double-precision
AArch64 instruction, captures guest FPSR, restores host state, and commits the
scalar result through the normal TCTI SIMD register path. KUnit exhausts every
source and destination register field and both precisions, then proves aliases,
ordinary results, numeric-NaN versus propagating-NaN behavior, signed zero,
signaling NaN with FPCR default-NaN mode, sticky FPSR state, upper-bit clearing,
unrelated SIMD and integer state, and exact PC progression. The app-hosted KUnit
and Linux kselftest gate reached `ORLIX-KSELFTEST-END`; XCTest passed one test
with zero failures and zero skips in 7.701 seconds, and `xcodebuild` exited zero
after its known `simctl diagnose` cleanup child was released. Patch-only Linux
`checkpatch.pl` reported zero errors and zero warnings. LLVM AArch64 encodings
and OpenMinis ish-arm64 `math.S` at commit
`89269e6fef7ab7aa61b133deae90d78e34a09ed1` were independent encoding and
family-decomposition cross-checks. No external implementation source was
copied.

## [2026-07-21] implement | Add AdvSIMD floating-point three-same execution

Added baseline A64 scalar and vector `FABD`, `FACGE`, `FACGT`, `FCMEQ`, `FCMGE`, `FCMGT`, `FMULX`, `FRECPS`, and `FRSQRTS` decode and execution without removing existing instruction families or prematurely promoting the broader `ASIMD_SCALAR` and `ASIMD_VECTOR_3SAME` inventory rows. The Orlix-owned ISA-on-ISA path saves host FPCR and FPSR, installs guest FP state with preemption disabled, executes the exact AArch64 scalar or AdvSIMD instruction for every legal single-precision and double-precision shape, captures guest FPSR, restores host state, and commits the architectural result through the normal TCTI register path. KUnit covers every source and destination register field, selector overlap boundaries, scalar and vector widths, source and destination aliases, exact ordinary results, zero and infinity special cases, quiet and signaling NaNs, IOC accumulation, default-NaN FPCR behavior, sticky QC, upper-bit clearing, unrelated register state, and PC progression. App-hosted KUnit passed 363 of 363 cases, Linux kselftest emitted `ORLIX-KSELFTEST-END`, XCTest passed in 8.101 seconds, and the owning gate exited 0. LLVM AArch64 TableGen commit `de44ed3488693686dd1f65baad5d7bd3797eddca` and OpenMinis ish-arm64 `math.S` commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` were independent family and decomposition cross-checks. No external implementation source was copied.

## [2026-07-21] implement | Add AdvSIMD pairwise long arithmetic

Added baseline A64 AdvSIMD `SADDLP`, `UADDLP`, `SADALP`, and `UADALP` decode and execution without removing existing instruction families or prematurely promoting the broader `ASIMD_VECTOR_2REG_MISC` inventory row. Decoder KUnit covers all 1,024 combinations of operation, signedness, vector width, element size, and register field, including reserved 64-bit source-element encodings. Production-path execution KUnit covers every legal vector shape, widening signed and unsigned pair sums, accumulating tied destinations, destination-source aliases, 64-bit upper-half clearing, unrelated SIMD and integer state preservation, and exact PC progression. App-hosted KUnit passed 360 of 360 cases, Linux kselftest emitted `ORLIX-KSELFTEST-END`, XCTest passed in 8.660 seconds, and the owning gate exited 0. LLVM AArch64 TableGen commit `de44ed3488693686dd1f65baad5d7bd3797eddca` and OpenMinis ish-arm64 `math.S` commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` were independent family cross-checks. No external implementation source was copied.

## [2026-07-21] implement | Add mixed-sign saturating add

Added baseline A64 AdvSIMD scalar and vector `SUQADD` and `USQADD` decode and execution without removing existing instruction families or prematurely promoting the broader `ASIMD_SCALAR` and `ASIMD_VECTOR_2REG_MISC` inventory rows. The production path handles every architectural lane width and vector shape, tied destination semantics, signed-to-unsigned and unsigned-to-signed saturation boundaries, sticky FPSR QC, scalar upper-bit clearing, 64-bit vector upper-half clearing, unrelated register preservation, and exact PC progression. App-hosted KUnit passed 358 of 358 cases, Linux kselftest emitted `ORLIX-KSELFTEST-END`, XCTest passed in 8.841 seconds, and the owning gate exited 0. LLVM AArch64 encoding output independently confirmed the supported instruction encodings. No external implementation source was copied.

## [2026-07-21] implement | Add scalar saturating doubling multiply-long

Added baseline A64 AdvSIMD scalar `SQDMULL`, `SQDMLAL`, and `SQDMLSL` decode and execution without promoting the broader `ASIMD_SCALAR` inventory row. The decoder accepts the architectural halfword-to-word and word-to-doubleword forms across every register field and rejects reserved size encodings. KUnit executes zero, unit, negative, and signed-boundary operands across distinct, source-destination, shared-source, and all-register aliasing; verifies signed doubling, accumulation and subtraction, saturation and sticky FPSR QC, full unrelated SIMD and integer state preservation, scalar upper-bit clearing, and exact PC progression. App-hosted KUnit passed 355 of 355 cases, Linux kselftest emitted `ORLIX-KSELFTEST-END`, and XCTest passed in 7.579 seconds. LLVM AArch64 TableGen commit `de44ed3488693686dd1f65baad5d7bd3797eddca` supplied the independent scalar-family encoding reference. OpenMinis ish-arm64 `math.S` at commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` independently confirmed the widening operation family; no implementation source was copied. RDM-only `SQRDMLAH` and `SQRDMLSH` remain outside the advertised guest profile because `ELF_HWCAP2` is zero.

## [2026-07-21] test | Complete AdvSIMD shift-immediate coverage

Audited the existing A64 AdvSIMD integer shift-immediate implementation against LLVM AArch64 TableGen commit `de44ed3488693686dd1f65baad5d7bd3797eddca` and promoted only the kernel-owned integer shift row. Direct KUnit execution covers scalar and vector `SHL`, `SLI`, `SRI`, `SSHR`, `USHR`, `SSRA`, `USRA`, `SRSHR`, `URSHR`, `SRSRA`, `URSRA`, `SQSHL`, `UQSHL`, `SQSHLU`, all eight vector narrowing forms including `SQSHRUN` and `SQRSHRUN`, all six scalar saturating narrowing forms, and `SSHLL` and `USHLL`, across every legal element size, vector width, scalar form, and shift amount. Scaled floating-point conversions remain explicitly partial under `FP_FIXED_POINT_CONVERT`; this promotion does not absorb or conceal them. App-hosted KUnit passed 353 of 353 cases, Linux kselftest emitted `ORLIX-KSELFTEST-END`, and XCTest passed in 7.712 seconds. The kernel-owned inventory now reports 38 of 52 families complete with 14 explicit gaps.

## [2026-07-21] test | Complete AdvSIMD modified-immediate coverage

Closed the A64 AdvSIMD `MOVI`, `MVNI`, `ORR` immediate, `BIC` immediate, and floating-point immediate family through production decode and execution. KUnit now enumerates every legal `Q`, `op`, `cmode`, `imm8`, and destination-register field, verifies exact independent immediate expansion, preserves unrelated SIMD and integer state, checks exact PC progression, and rejects the unexposed FP16 `o2` family. The exhaustive proof exposed and fixed an early shift-left-long guard that intercepted valid `cmode=10` modified-immediate encodings. The complete shift-left-long regression remained enabled and passed after its two overlapping `immh=0` words were assigned to their architecturally valid modified-immediate class; genuinely reserved `immh=8` remains rejected. App-hosted KUnit passed 353 of 353 cases, Linux kselftest emitted `ORLIX-KSELFTEST-END`, and XCTest passed in 7.519 seconds. The kernel-owned inventory now reports 37 of 52 families complete with 15 explicit gaps. LLVM AArch64 TableGen commit `de44ed3488693686dd1f65baad5d7bd3797eddca` and OpenMinis ish-arm64 commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` were used only as independent encoding and family cross-checks; no implementation source was copied.

## [2026-07-21] test | Complete AdvSIMD copy coverage

Closed A64 AdvSIMD scalar and vector element `DUP`, general-register `DUP`, general-register and element `INS`, `UMOV`, and `SMOV` through production decode and execution. KUnit now enumerates every legal element size, lane, vector width, source and destination register field, same-register and same-lane aliases, GPR 31 zero and discard behavior, exact signed and unsigned transfer results, unrelated SIMD and integer state preservation, SP and PSTATE preservation, exact PC progression, and reserved encodings. The exhaustive proof exposed and fixed the 64-bit same-register same-lane `INS` fast path leaving guest SIMD state invalid. App-hosted KUnit cases 254 and 255 passed, Linux kselftest emitted `ORLIX-KSELFTEST-END`, and XCTest passed in 8.345 seconds. The kernel-owned inventory reports 36 of 52 families complete with 16 explicit gaps.

## [2026-07-21] test | Complete AdvSIMD permute coverage

Closed A64 AdvSIMD `UZP1`, `UZP2`, `TRN1`, `TRN2`, `ZIP1`, and `ZIP2` through production decode and execution. KUnit now enumerates every three-bit opcode slot, both vector widths, every element-size encoding, and every source and destination register field. It proves exact lane selection, source-source and destination-source aliases, 64-bit upper-half clearing, unrelated SIMD and integer state preservation, exact PC progression, rejection of both reserved opcode slots, and rejection of the illegal 64-bit `size=3` form. The kernel-owned inventory reports 35 of 52 families complete with 17 explicit gaps.

## [2026-07-21] test | Complete AdvSIMD table lookup coverage

Closed A64 AdvSIMD `TBL` and `TBX` through production decode and execution. KUnit now proves both result widths, one through four consecutive table registers including `V31` wraparound, every table-base, index, and destination register field, valid and out-of-range indexes, index-table, destination-table, and destination-index aliases, `TBL` zero fill, `TBX` destination retention, 64-bit upper-half clearing, unrelated SIMD and integer state preservation, and exact PC progression. The kernel-owned inventory reports 34 of 52 families complete with 18 explicit gaps.

## [2026-07-21] test | Complete AdvSIMD extract coverage

Closed A64 AdvSIMD `EXT` through production decode and execution. KUnit now proves every legal byte offset for 64-bit and 128-bit vectors, every SIMD source and destination register field, source-source and destination-source aliases, exact concatenation ordering, 64-bit upper-half clearing, unrelated SIMD and integer state preservation, exact PC progression, and rejection of the reserved 64-bit offset range. The kernel-owned inventory reports 33 of 52 families complete with 19 explicit gaps.

## [2026-07-21] test | Complete AES instruction coverage

Closed A64 `AESE`, `AESD`, `AESMC`, and `AESIMC` through production decode and execution. KUnit now proves every SIMD source and destination register field, source-destination aliases, all 256 byte-substitution inputs against a table-based oracle independent of the production algebra, forward and inverse row and column transforms, unrelated SIMD and integer state preservation, and exact PC progression while retaining the earlier known vectors. The kernel-owned inventory reports 32 of 52 families complete with 20 explicit gaps. OpenMinis ish-arm64 commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` was used only to cross-check instruction decomposition and standard AES transformations. No OpenMinis production implementation was copied.

## [2026-07-21] test | Complete polynomial multiply

Closed A64 `PMUL`, `PMULL`, and `PMULL2` through production decode and execution. KUnit proves byte-lane low polynomial products, byte-to-halfword widening, 64-bit-to-128-bit carryless products, both `Q` source halves, every SIMD source and destination register field, source and destination aliases, unrelated SIMD and integer state preservation, exact PC progression, and rejection of reserved element sizes. The kernel-owned inventory reports 31 of 52 families complete with 21 explicit gaps. OpenMinis ish-arm64 commit `89269e6fef7ab7aa61b133deae90d78e34a09ed1` was used only as an independent encoding and decomposition cross-check; no source was copied.

## [2026-07-21] test | Complete CRC32 and CRC32C

Closed all eight A64 `CRC32` and `CRC32C` variants through production decode and execution. KUnit proves every byte, halfword, word, and doubleword source form against an independent reflected-polynomial oracle, covers every source, accumulator, and destination register field including zero-register and alias behavior, verifies 32-bit result extension, and preserves unrelated registers, SP, NZCV, and exact PC progression. The kernel-owned inventory reports 30 of 52 families complete with 22 explicit gaps.

## [2026-07-20] test | Complete A64 exception generation

Closed the A64 exception-generation family for the guest EL0 profile. KUnit covers every `SVC` and `BRK` immediate encoding, verifies immediate extraction and structured syscall and breakpoint exits, and rejects all other `op1` and `LL` classes including `HVC`, `SMC`, `HLT`, `DCPS1`, `DCPS2`, `DCPS3`, and unallocated combinations. The kernel-owned inventory reports 29 of 52 families complete with 23 explicit gaps. LLVM AArch64 TableGen at commit `f1073034a030a09bf0a29607aa0bee4430b31b97` was used only as an independent encoding cross-check; no source was copied.

## [2026-07-20] test | Complete scalar register data processing families

Closed logical shifted register, add/subtract extended register, data-processing one-source, two-source, and three-source families through production decode and execution. Corrected flag-setting extended-register forms to read register 31 as SP, and added exhaustive KUnit coverage for architectural operations, widths, register fields, aliases, reserved encodings, state preservation, PC progression, and NZCV where applicable. The kernel-owned inventory reports 28 of 52 families complete with 24 explicit gaps.

## [2026-07-20] test | Complete add/subtract shifted register

Closed the A64 `ADD`, `ADDS`, `SUB`, and `SUBS` shifted-register family and its `CMP`, `CMN`, and `NEG` aliases through production decode and execution. KUnit proves both widths, LSL, LSR, and ASR at every legal shift, the complete operation and flag matrix, every register field including zero-register and alias cases, independent arithmetic and NZCV results, and rejection of ROR and oversized 32-bit shifts. The kernel-owned inventory reports 23 of 52 families complete with 29 explicit gaps.

## [2026-07-20] test | Complete bitfield

Closed the A64 `SBFM`, `BFM`, and `UBFM` family and their shift, extend, insert, and extract aliases through production decode and execution. KUnit executes every legal W/X `immr:imms` pair for all three operations against an independent architectural `wmask` and `tmask` oracle, proves every source and destination field including zero-register and alias cases, verifies sign extension, destination preservation, and 32-bit zero extension, and rejects reserved opcode, width, and immediate encodings. The kernel-owned inventory reports 22 of 52 families complete with 30 explicit gaps.

## [2026-07-20] test | Complete extract

Closed the A64 `EXTR` family and its `ROR` aliases through production decode and execution. KUnit enumerates every `sf:N:shift` combination, rejects width mismatches and illegal 32-bit shifts, executes every legal shift in both widths, proves every source and destination field including zero-register and alias cases, verifies 32-bit zero extension, and preserves flags, stack pointer, and exact PC progression. The kernel-owned inventory reports 21 of 52 families complete with 31 explicit gaps.

## [2026-07-20] test | Complete logical immediate

Closed the A64 `AND`, `ORR`, `EOR`, and `ANDS` immediate family through production decode and execution. KUnit exhaustively enumerates the full `sf:N:immr:imms` encoding space for every operation, compares valid masks with an independent bitmask oracle, rejects every invalid encoding, proves every source and destination field including zero-register aliases, verifies 32-bit zero extension and ANDS flags, and preserves stack pointer and exact PC progression. The kernel-owned inventory reports 20 of 52 families complete with 32 explicit gaps.

## [2026-07-20] test | Complete move-wide immediate

Closed the A64 `MOVN`, `MOVZ`, and `MOVK` family through production decode and execution. KUnit proves both widths, every legal halfword position, immediate and destination fields including zero-register behavior, MOVK field preservation, 32-bit zero extension, preserved stack pointer and flags, exact PC progression, and rejection of the reserved opcode and illegal 32-bit halfword positions. The kernel-owned inventory reports 19 of 52 families complete with 33 explicit gaps.

## [2026-07-20] test | Complete add/subtract immediate

Closed the A64 `ADD`, `ADDS`, `SUB`, and `SUBS` immediate family through production decode and execution. KUnit proves both widths, both immediate shifts, all immediate and register fields including SP and compare aliases, independent result and NZCV calculation across boundary operands, 32-bit zero extension, preserved architectural state, exact PC progression, and rejection of reserved shift encodings. The kernel-owned inventory reports 18 of 52 families complete with 34 explicit gaps.

## [2026-07-20] fix | Complete add/subtract with carry

Closed the A64 `ADC`, `ADCS`, `SBC`, and `SBCS` family through production decode, lowering, and gadget execution. KUnit proves both widths, both carry inputs, every register field including zero-register and alias cases, independent result and NZCV calculation across boundary operands, preserved architectural state, exact PC progression, and rejection of reserved encoding bits. The kernel-owned inventory reports 17 of 52 families complete with 35 explicit gaps.

## [2026-07-20] fix | Complete integer conditional compare

Closed the A64 `CCMP` and `CCMN` family through production decode, lowering, and gadget execution. KUnit proves both widths, register and immediate forms, every condition across every current and fallback NZCV value, all register and immediate fields, zero-register semantics, independent addition and subtraction flag results, false-condition fallback, preserved registers and stack pointer, and exact PC progression. The decoder now requires the architectural `S` bit and rejects the reserved bit 10 and bit 4 encodings. The kernel-owned inventory reports 16 of 52 families complete with 36 explicit gaps.

## [2026-07-20] fix | Complete integer conditional select

Closed the A64 `CSEL`, `CSINC`, `CSINV`, and `CSNEG` family through production decode, lowering, and gadget execution. KUnit proves both widths, every condition and NZCV combination, every register field, zero-register behavior, destination-source aliasing, false-path transforms, 32-bit zero extension, preserved flags and stack pointer, and exact PC progression. The decoder now rejects the reserved `S` and fixed `op2` bits instead of treating them as legal selects. The kernel-owned inventory reports 15 of 52 families complete with 37 explicit gaps.

## [2026-07-20] fix | Complete unconditional register branches

Closed the A64 `BR`, `BLR`, and `RET` family through production decode, lowering, and gadget execution. KUnit proves every source register including `X30` and `XZR`, exact targets, `BLR` link updates, preserved general registers, stack pointer and flags, and rejection of malformed fixed fields. The implementation now snapshots the branch target before writing `X30`, fixing `BLR X30` aliasing. The kernel-owned inventory reports 14 of 52 families complete with 38 explicit gaps.

## [2026-07-20] implement | Complete test-and-branch immediate

Closed the A64 `TBZ` and `TBNZ` family through production decode, lowering, and gadget execution. KUnit proves all 64 selectable bit positions, every source register including `XZR`, both branch polarities, set and clear source states, signed 14-bit displacement boundaries and individual immediate bits, taken and fallthrough behavior, preserved architectural state, and exact PC results. The decoder now sign-extends the already-scaled displacement, avoiding a signed negative left shift in C. The kernel-owned inventory reports 13 of 52 families complete with 39 explicit gaps.

## [2026-07-20] implement | Complete compare-and-branch immediate

Closed the A64 `CBZ` and `CBNZ` family through production decode, lowering, and gadget execution. KUnit proves 32-bit and 64-bit forms, every source register including `WZR` and `XZR`, low-word versus full-register zero semantics, both branch polarities, signed 19-bit displacement boundaries and individual immediate bits, taken and fallthrough behavior, preserved architectural state, and exact PC results. The decoder now sign-extends the already-scaled displacement, avoiding a signed negative left shift in C. The kernel-owned inventory reports 12 of 52 families complete with 40 explicit gaps.

## [2026-07-20] implement | Complete conditional immediate branches

Closed the A64 `B.cond` family through production decode, lowering, and gadget execution. KUnit proves all 16 condition codes across all 16 NZCV states, taken and fallthrough behavior, signed 19-bit immediate boundaries and individual immediate bits, the reserved encoding bit, preserved general registers, stack pointer and flags, and exact PC results. The decoder now sign-extends the already-scaled displacement, avoiding a signed negative left shift in C. The kernel-owned inventory reports 11 of 52 families complete with 41 explicit gaps.

## [2026-07-20] implement | Complete unconditional immediate branches

Closed the A64 `B` and `BL` immediate family through production decode, lowering, and gadget execution. KUnit proves both link modes, signed 26-bit immediate boundaries and individual immediate bits, exact branch targets, `X30` link updates, preserved general registers, stack pointer and flags, and no fallthrough PC increment. The decoder now sign-extends the already-scaled branch displacement, avoiding a signed negative left shift in C. The kernel-owned inventory reports 10 of 52 families complete with 42 explicit gaps.

## [2026-07-20] implement | Complete PC-relative addressing

Closed the A64 `ADR` and `ADRP` family through production decode, lowering, and gadget execution. KUnit now proves every destination register, including discarded writes to `XZR`, signed 21-bit immediate boundaries and individual immediate bits, ADRP page alignment, preserved general registers, stack pointer and flags, and exact PC progression. The decoder now sign-extends the already-scaled ADRP immediate, avoiding a signed negative left shift in C. The kernel-owned inventory reports 9 of 52 families complete with 43 explicit gaps.

## [2026-07-20] implement | Complete scalar fused three-source FP

Rejected reserved scalar FP three-source type values and proved `FMADD`, `FMSUB`, `FNMADD`, and `FNMSUB` for both precisions across every SIMD register field. KUnit covers destination-source aliasing, fused-only results that differ from separate multiply and add, invalid-operation NaN and FPSR state, upper-lane clearing, source preservation, and exact PC progression through production lowering and gadget execution. The scalar compare and conditional-compare family proofs now use that same production path instead of calling decoded semantics directly.

## [2026-07-20] verify | Complete scalar FP compare coverage

Exhaustively proved `FCMP` and `FCMPE` register and zero forms across both precisions, all SIMD source-register fields, quiet and signaling NaNs, signed zero, infinities, NZCV results, accumulated FPSR IOC state, operand preservation, and PC progression. LLVM disassembly independently confirmed that every encoded `Rm` value in the zero form is valid and canonicalizes to comparison with `#0.0`, so TCTI preserves those encodings rather than narrowing the ISA.

## [2026-07-20] implement | Complete scalar FP conditional compare

Moved `FCCMP` and `FCCMPE` recognition ahead of overlapping broad AdvSIMD decoder groups, then proved both precisions and signaling forms across every condition, current NZCV state, fallback NZCV value, and SIMD source register. KUnit also proves quiet-NaN and signaling-NaN IOC behavior, false-condition suppression, reserved type rejection, unrelated-state preservation, and exact PC progression.

## [2026-07-20] implement | Complete scalar FP conditional select

Moved `FCSEL` recognition ahead of overlapping broad AdvSIMD decoder groups, then exhaustively proved all 16 condition codes against all 16 NZCV states for single and double precision through production lowering and gadget execution. KUnit also proves destination-source aliasing, reserved type rejection, upper-lane clearing, and exact PC progression.

## [2026-07-20] implement | Complete scalar FP immediate execution

Separated scalar `FMOV` immediate from the AdvSIMD modified-immediate class, implemented a dedicated production semantic path, and exhaustively proved all 256 immediate encodings for single and double precision through TCTI lowering and gadget execution. Reserved type encodings remain unsupported and the coverage inventory closes only the scalar FP immediate family.

## [2026-07-20] implement | Complete scalar FP two-source execution

Replaced scalar floating-point two-source opcode special cases with one architectural family decoder, added all Armv8.0-A operations for single and double precision, rejected reserved type and opcode encodings, and proved minimum, maximum, numeric-NaN, signed-zero, and negated-multiply state transitions in KUnit. The kernel-owned coverage inventory now closes that family while retaining every other open gap.

## [2026-07-20] audit | Add the kernel-owned TCTI ISA inventory

Added a repo-owned A64 instruction-family inventory under `arch/orlix`, tied it to production decoder classes and existing KUnit evidence, and made KUnit report a ratcheted nonzero gap count until every required family is closed. Partial rows remain explicit and do not cite tests that do not exist.

## [2026-07-19] define | Bind TCTI coverage to the guest ISA profile

Declared `arch/orlix/include/asm/isa.h` as the current Orlix EL0 contract for Armv8.0-A with floating point and AdvSIMD, made the Linux ELF HWCAP surface consume that declaration, and required the same guest ISA contract across development, release, simulator, and device destinations. Optional extensions remain unadvertised until their complete instruction families and exception boundaries have owning KUnit proof.

## [2026-07-19] advance | Start complete AArch64 ISA-on-ISA coverage

Moved the complete AArch64 ISA-on-ISA coverage task into active work and closed the baseline A64 load-literal family across integer, sign-extending, SIMD/FP, prefetch, and unallocated encodings. Completed non-temporal integer and SIMD pair decoding, including 32-bit SIMD pairs and the unallocated non-temporal LDPSW boundary. The app-hosted kernel gate remains the owning proof surface through KUnit and Linux kselftest. Full ISA coverage remains open until the guest profile inventory and every required family have zero decode, lowering, semantic, and exception gaps.

## [2026-07-19] test | Keep Linux assertions in native TAP suites

Reduced app-hosted kernel conformance XCTest cases to session launchers. Focused launchers now validate the selected kselftest through structured TAP identity, while Linux behavior assertions remain in KUnit and kselftest.

## [2026-07-19] test | Return time behavior to owning suites

Added a Linux kselftest for kernel time surfaces and focused OrlixMLibC tests for calendar conversion and composite formatting. The app-hosted XCTest now launches the kernel probe without duplicating its TAP assertions.

## [2026-07-19] clarify | Keep TCTI inside the Linux runtime boundary

Made the short component pages explicit that OrlixKernel remains the Linux runtime and owns kernel semantics, while TCTI is the complete AArch64 EL0 guest instruction-execution backend under `arch/orlix` required by iOS executable-memory restrictions. Linked the boundary directly to ADR 0022 without creating a duplicate policy source.

## [2026-07-17] constrain | Require complete AArch64 ISA-on-ISA coverage

Made complete guest-exposed AArch64 EL0 ISA coverage a blocking TCTI architecture and promotion requirement. Workload opcodes now serve only as prioritization and regression evidence. Added a dedicated task for architectural decode, production lowering and gadget execution, exact state semantics, structured exceptions, KUnit and kselftest proof, and an independent coverage audit informed by the reviewed OpenMinis reference without copying its implementation.

## [2026-07-16] correct | Return TCTI proof to owning tests

Removed the centralized Swift gate, golden-ELF and reducer workflow, TCTI-specific product profile, and proposed TCTI report MCP. Routed structured engine proof to KUnit, Linux-visible behavior to kselftest, libc and package behavior to upstream suites, private Darwin mechanics to HostAdapter XCTest, and product integration to OrlixOS and native app XCTest. The remaining task envelope records scope and order without interpreting results.

## [2026-07-15] start | Bind local session terminal geometry

Moved the local-session binding task into doing and recorded the current ownership limit: OrlixOS exposes an instance-shaped session API, while the hosted kernel, boot progress, console input, and active HostAdapter output registration remain process-global.

## [2026-07-15] model | Replace initiatives with a work hierarchy

Replaced the flat initiative object type with a strict `epic -> story -> task` hierarchy. Added `todo`, `doing`, and `done` status folders for every work type, migrated durable work and its consumers, and made lifecycle hooks load the complete doing hierarchy before mutations.

## [2026-07-15] correct | Preserve escaped source links

Clarified that Markdown links are resolved from the linking page under `docs/` and may escape to repository source files when the resolved target remains inside the repository. Restored clickable source links in migrated capability material and kept absolute local paths forbidden.

## [2026-07-15] cutover | Make ontology authoritative

Moved release and TCTI provenance into typed sources, updated release and TCTI consumers atomically, changed agent context loading from plan journals to active initiative pages and structured reports, added ontology maintenance skills and lint gates, and regenerated the index. Current execution evidence remains under `Build/AgentHarness/` and is intentionally absent from authored status prose.

## [2026-07-15] migrate | Normalize authored Orlix knowledge

Preserved ADR 0001 through ADR 0028 as typed architecture-decision objects, synthesized architecture and glossary material into reusable concepts, converted durable active work into initiative objects, and converted application specifications into lifecycle-owned capability or concept pages. Removed stale implementation journals, handoff archives, templates, harness memory, and the retired non-Apple shell-bridge backlog. Exact chronology remains available through Git.

This append-only log records meaningful knowledge-base actions. Exact file history remains in Git.

## [2026-07-15] reorg | Adopt the Orlix ontology brain

Started the migration from narrative architecture, ADR, plan, handoff, harness-memory, release, and application-spec trees to one typed knowledge graph rooted at `docs/`.

## [2026-07-15] correct | Use one terminal multiplex protocol

Replaced boot-command-line terminal geometry and printable resize markers with the versioned binary-safe terminal multiplex protocol, and recorded Linux-console-derived interactive source selection.

## [2026-07-15] track | Separate app-hosted XCTest cleanup

Added a bounded follow-up task for successful app-hosted runtime XCTest runs that leave `xcodebuild` waiting in test-session cleanup. Preserved the accepted Linux console-policy and terminal-transport behavior as regression constraints rather than reopening their implementation.

## [2026-07-15] resolve | Make app-hosted XCTest cleanup deterministic

Replaced semaphore polling in the app-hosted runtime proof with XCTest-native expectations, preventing the priority-inversion diagnostic that left `xcodebuild` waiting for asynchronous simulator diagnostics after a successful test. Closed the focused cleanup task without changing Linux, HostAdapter, session, console-policy, multiplex, or terminal-geometry behavior.

## [2026-07-15] model | Normalize roadmap priorities and dependencies

Split release work into mobile terminal, mobile container, and native macOS stages, separated the native application and Local Runtime stories, and recorded every epic, story, and task in one durable priority matrix. Added inverse dependency and cycle validation so the authored graph cannot silently drift from the documented execution order.

## [2026-07-24] correct | Separate TCTI target completion from current evidence

Corrected the TCTI target, promotion, and roadmap language so the 4,350-leaf target remains unfinished until its owning audit and per-family proof close. The scope envelope now records required proof tiers without presenting current results, includes every ADR 0017 tier, keeps runtime rejection distinct from target completion, reruns the app-hosted ladder only after coverage, and makes optional device validation independent of product-default promotion.

## [2026-07-24] define | Reconcile system-access semantics without changing the ISA denominator

Recorded the three-file Arm source contract and made `Instructions.json` the immutable 4,350 direct instruction-encoding denominator. `Registers.json` accessors now supply supplemental variants mapped through generic system-access leaves, so `RNDR` and `RNDRRS` do not create new instruction leaves. Missing, ambiguous, reserved, or contradictory accessor selector mappings are audit failures.

## [2026-07-24] implement | Establish the C-native complete-ISA audit foundation

Added the exact 4,350-leaf source manifest, explicit target classification and proof ledgers, typed feature and register import models, source-bound operation catalogs, and focused production-path KUnit coverage for the current scalar, atomic, and cryptographic families. The checkpoint keeps Arm JSON inputs in explicit host audit tooling, keeps ordinary kernel execution C-native, and records unresolved classifications and semantic domains as blocking gaps rather than completion evidence.

## [2026-07-24] correct | Make canonical C artifacts the TCTI inventory boundary

Restricted raw Arm source import to an explicit maintainer refresh operation and made canonical fixed-width C artifacts under `arch/orlix` the only inventory inputs for normal kernel, KUnit, kselftest, product, and complete-target audit paths. Kconfig remains build and capability selection rather than an ISA inventory store. Added source-bound logical shifted-register and bitfield/EXTR KUnit coverage, corrected EXTR reserved-bit decoding, and retained unresolved classifications and missing production-path proof as blocking work.

## [2026-07-24] implement | Version the complete register source inside arch/orlix

Published the pinned Arm register topology as durable C-native source under `arch/orlix`, including every register, metadata record, permission, field, value domain, constraint, accessor, selector, and typed expression relationship. The external Arm JSON remains an explicit maintainer-refresh input only. Normal kernel, KUnit, kselftest, product, and complete-target audit paths require no JSON parser or external source path, while the completion audit continues to report every unclassified and unproved instruction leaf as blocking.

## [2026-07-24] define | Require authoritative semantic provenance for every TCTI leaf

Made the pinned Arm AARCHMRS 2026-06 package authoritative for the 4,350-leaf target and pinned official Arm ASL authoritative for per-leaf semantics. Every leaf now requires a machine-checkable source, feature, semantic, classification, implementation, KUnit, applicable kselftest, runtime-advertisement, implementation-status, and proof-status chain. Missing or ambiguous edges fail closed. Linux remains authoritative only for Linux integration, while external implementations and formal-method references may inform investigation or proof design without becoming semantic or runtime dependencies.

## [2026-07-25] implement | Bind authoritative TCTI inventory gaps into the audit

Extended the C-native TCTI inventory and completion audit with exact source ordinals, static proof bindings, runtime-advertisement separation, feature-condition validation, immutable six-artifact refresh publication, complete A64 system-accessor reconciliation, and explicit shared-ASL availability failure. The checkpoint keeps all 4,350 leaves visible, reports exact feature applicability and semantic provenance as unresolved blockers, preserves zero unproved HWCAP advertisement, and adds focused kernel-owned regressions without treating static source ownership or terminal output as executed proof.

## [2026-07-25] harden | Separate TCTI provenance from execution proof

Bound every direct instruction leaf to its raw pinned source span, made the maintainer refresh reject drift from checked C artifacts, integrated all supplemental system-accessor rows and the missing shared-ASL witness into the canonical audit, and required the runtime projection to retain the complete 4,350-leaf denominator. Expanded static ownership for the implemented LSE families while keeping every architectural duty unresolved until typed native KUnit and applicable kselftest results exist.

## [2026-07-25] harden | Authenticate TCTI source-derived semantic edges

Made the completion audit reject one-byte direct-source span drift, authenticated the selector payload and condition identity of all 2,014 supplemental system accessors, and retained exact operational-note presence, span, and digest state for every shared-ASL absence witness. Removed an unconditional skipped KUnit placeholder and kept that semantic gap in the authoritative provenance audit until official shared ASL is separately pinned.

## [2026-07-25] harden | Preserve typed feature qualifiers and proof boundaries

Recorded the current complete-target checkpoint without reducing the
4,350-leaf target: the checked feature artifact retains 605 field qualifier
occurrences, while the maintainer binding core derives 362 identity groups,
maps 604 occurrences, and leaves one ambiguity as a blocking audit failure.
Runtime projection now rejects
stale zero-profile mappings and archive validation rejects stale kernel output.
A narrow native observation comparator records typed execution observations but
cannot close an obligation without owning production-path KUnit evidence.
LSE128 reserved-selector and near-miss coverage remains pending owning run
evidence and does not change completion status.

## [2026-07-25] harden | Retain field and alias obligations in the complete target

Extended the checkpoint with typed 1 through 128-bit feature values and a
checked 605-occurrence feature-field artifact. Its 604 structural mappings
remain semantically unresolved, while the one `MPAM` ambiguity remains an
explicit audit blocker. Preserved 292 instruction aliases and 171 reachable
operation aliases as supplemental source relationships without changing the
4,350 direct-leaf denominator or treating aliases as proof. The pinned 2026-06
Arm package still has no separately pin-able official shared-ASL corpus, so all
4,350 semantic provenance edges remain blocking. Expanded the LSE128
production resume fault matrix without making an atomicity or ordering claim,
and kept runtime HWCAP and HWCAP2 promotion at zero.

## [2026-07-25] harden | Bind runtime capability candidates without promotion

Added a checked C-native cohort artifact for all 4,350 direct instruction
leaves, retaining 5,592 source-derived candidate memberships over 409 typed
feature parameters with exact condition spans. Every candidate remains
unresolved and blocks the complete-target audit. Runtime projection validates
the artifact but cannot use structural membership to promote HWCAP or HWCAP2,
so feature applicability, satisfiability, implementation, and owning proof
remain explicit open obligations.

## [2026-07-25] decide | Pin authoritative TCTI semantic and proof sources

Enhanced ADR 0022 to make pinned Arm AARCHMRS 2026-06 the sole 4,350-leaf
target authority and official Arm shared ASL the semantic authority. Recorded
the required C-native per-leaf source-to-proof graph, hard failure for missing
classification or ownership, Linux-only integration references,
verification-methodology references, and the prohibition on external semantic
or proof oracles.

## [2026-07-25] verify | Compile reviewed TCTI regressions without promoting proof

Validated the C-native 4,350-leaf host audit and authenticated proof registry,
then built the KUnit-enabled iOS Simulator product archive with reviewed scalar
LSE RMW, literal-load and PRFM, and AdvSIMD modified-immediate regression
suites. The truthful audit remains incomplete at 1,085 classified and 3,265
unclassified leaves, with 4,350 missing official shared-ASL provenance edges,
4,057 unresolved feature conditions, 1,044 source-binding failures, 196 stale
proof bindings, and 417 bindings with unproved obligations. App-hosted KUnit
and kselftest execution were not attempted and no runtime
capability was promoted. The generated knowledge index was refreshed with the
checkpoint.

## [2026-07-25] test | Bind TCTI conditions to generated operand metadata

Added a C-native, fail-closed operand assignment seam for source-condition
evaluation. The focused ordinal 3297 regression validates the canonical
instruction artifact, derives `STRB_32B_ldst_regoff` option bits from generated
operand metadata, and rejects malformed metadata or encodings that disagree
with the generated leaf mask and pattern. This is applicability infrastructure
only. It grants no instruction-semantic or proof credit, and the complete-target
audit remains blocked by 116 unresolved operand conditions, 3,941 missing
feature configurations, and 4,350 unavailable official shared-ASL bodies.

## [2026-07-25] test | Retain fixed instruction-field provenance

Extended the pinned AARCHMRS importer with a separate C-native fixed-operand
provenance plane for named all-fixed encoding fields. Focused tests bind inherited
`opc` values for `orr_z_zi_`, `eor_z_zi_`, and `and_z_zi_` to exact leaf-local
source objects, and prove fixed and partially variable fields remain disjoint.
This checkpoint records source provenance only. It grants no feature-applicability,
semantic, implementation, KUnit, kselftest, or runtime-advertisement credit.

## [2026-07-25] test | Bind ERETAA and ERETAB EL0 rejection

Classified pinned source ordinals 2300 and 2301 as non-EL0 and bound both
`ERETA` leaves to one source-specific rejection proof. Production-path KUnit
drives each instruction through `tcti_resume_user`, requires the structured
unsupported result, and verifies unchanged architectural state. The registry
now supports multiple exact source bindings for one operation while rejecting
mixed-operation bindings. Both leaves remain ASL-blocked with all semantic
obligations unproved.

## [2026-07-25] build | Publish fixed instruction-field artifact

Published 16,675 imported all-fixed instruction fields in the versioned
C-native target instruction artifact. Each leaf now has a validated contiguous
fixed-field span with exact condition, bit range, mask, value, source span, and
pinned-source locator. Canonical tests cover `opc` on ordinals 203 through 205
and `op21` on ordinals 2169 and 2170, plus adversarial artifact mutations. This
remains provenance infrastructure and grants no semantic or proof credit.

## [2026-07-25] test | Project every TCTI target leaf obligation

Added a transactional C-native projection for all 4,350 target leaves from the
canonical completion audit. Each ordinal retains source identity, classification,
ASL availability, known feature-union reasons, proof metadata, unproved duties,
execution-evidence blockers, and runtime-candidate provenance. Malformed source,
classification, or registry inputs leave caller output untouched. Every row
remains blocked on the incomplete authoritative feature-configuration union,
and static KUnit registration cannot become execution proof.

## [2026-07-25] test | Bind PMUL target reachability

Added source ordinal 3955, `PMUL_asimdsame_only`, to the durable crypto target
contract as implemented but blocked on official shared ASL. Focused KUnit binds
the canonical source identity, feature condition, mask and pattern, ASL locator,
decoder shape, and production executor PC reachability without introducing a
PMUL result oracle. The KUnit object compiles, while app-hosted execution remains
unproved because it was not run.

## [2026-07-25] test | Resolve fixed source-condition operands

Extended the C-native feature-domain operand resolver to consume validated fixed
fields from the generated target artifact. Focused tests resolve exact `opc`
values on ordinals 203 through 205 and `op21` on 2169 and 2170, then evaluate
their authoritative source conditions. Adversarial cases reject malformed
source locators, interior string-pool offsets, duplicate records, and
variable/fixed name collisions. This grants domain-provenance coverage only.

## [2026-07-25] test | Evaluate checked source-encoding witnesses

Changed the completion audit to resolve each source condition against the
validated instruction artifact's canonical encoding witness before reporting
operand gaps. This removes 114 false missing-operand blockers and exposes the
remaining feature-configuration blockers without treating one encoding witness
as a legal-domain or feature-union proof. All 4,350 leaves retain the explicit
incomplete-union blocker, and malformed or null source, classification,
registry, or instruction-artifact inputs remain transactionally invisible to
callers.
## [2026-07-26] fix | Retain typed ASL source provenance

Extended the C-native TCTI inventory to retain each leaf's canonical operation,
exact operation and decode source locators and spans, raw source digests, and
typed body, decode, corpus, and helper-availability states. The pinned 2026-06
source remains fail-closed because every operation body is a placeholder, every
decode body is null, and no official shared-ASL corpus or helper manifest is
present. This grants no instruction-semantic, proof, or runtime capability
credit.

## [2026-07-26] test | Expand production-path TCTI source coverage

Added mapped-RX `tcti_resume_user` coverage for the unprivileged load/store,
baseline exclusive, AdvSIMD table and permute, and AdvSIMD integer-halving
families. The exclusive suite binds all 24 baseline leaves to the checked Arm
source artifact and asserts acquire and release classification. Branch-target
fetch faults now report deterministic structured results in both production and
switch-debug execution. These checkpoints expand owning KUnit evidence without
advertising a capability or claiming complete target-ISA proof.

## [2026-07-26] decide | Canonicalize the public SDK and private layers

Made `Orlix` the sole product and app identity and `OrlixOS.xcframework` the
sole public SDK. Recorded private static OrlixKernel, OrlixMLibC, and
OrlixCoreUtils artifacts, private OrlixTCTI and OrlixHostAdapter execution,
framework-owned distribution resources without a payload bundle, OrlixMachine,
`OrlixOS.Containers`, the Herdr Session to Workspace to Tab to Pane hierarchy,
and the two private test hosts. Renamed the structured machine work without
compatibility aliases while keeping commercial Herdr access and Docker/Compose
behavior explicitly unfinished behind their existing todo tasks.

## [2026-07-26] refresh | Rebind application release inputs after flattening

Moved the release manifest's imported source, package resolution, vendor
artifacts, required evidence, capability evidence, privacy source, product
identity, and current documentation source references to the flattened
`Orlix/` tree. Recomputed the package-resolution and privacy hashes, revalidated
every vendored artifact and native-source hash, and recorded the lowercase app,
CloudKit, application-group, and Live Activity identifiers while preserving the
external StoreKit commerce identifiers.

## [2026-07-26] correct | Restore agreed private implementation identifiers

[CORRECTION] ADR 0030 and its derived component pages now identify the private
static implementation artifacts as `com.rudironsoni.orlix.os.kernel`,
`com.rudironsoni.orlix.os.mlibc`, and
`com.rudironsoni.orlix.os.coreutils`. The public OrlixOS SDK identifier remains
`com.rudironsoni.orlix.os`; no code identifiers changed in this documentation
correction.

## [2026-07-26] correct | Reconcile flattened app, TCTI, and entitlement references

[CORRECTION] Current application provenance and XcodeGen decisions now use the
flattened `Orlix/` root, OrlixTCTI ownership resolves to
`arch/orlix/hosted_exec/orlix_tcti`, and CloudKit plus app-group references use
the agreed lowercase identifiers. Historical forbidden-identity evidence and
external `com.rudironsoni.Orlix.pro.*` StoreKit commerce identifiers remain
unchanged.

Direct OrlixOS resource synchronization now uses checksum comparison after a
staging identity change, so same-size files changed within one timestamp tick
cannot leave stale framework resources behind.

## [2026-07-27] correct | Distinguish Arm semantic-source pinning from authorization

[CORRECTION] The earlier entries reporting no separately pin-able official Arm
shared-ASL corpus predated acquisition of Arm's external
`ISA_A64_xml_A_profile-2026-06` archive. The maintainer source contract now pins
and validates that package and records semantic availability for all 4,350
direct leaves: 4,332 semantic sections are present, 18 encodings are absent,
and zero records are incomplete. The archive is not redistributed. Its bundled
notice grants no intellectual property license, so technical provenance does
not establish authorization to use the corpus and grants no implementation,
proof, runtime-capability, or product-readiness credit.

## [2026-07-27] decide | Make is the sole executable developer interface

ADR 0019 now forbids standalone repository command scripts. Build, test,
release-validation, payload-staging, and TCTI artifact-refresh behavior is
owned by component Make rules, with only non-executable source modules behind
private targets. Generated wrappers required by upstream build systems remain
disposable `Build/` output.

## [2026-07-27] correct | Resolve every feature-field domain losslessly

[CORRECTION] The checked V3 feature-field-domain artifact now resolves all 605
qualifier occurrences across 362 identity groups. It retains 606 declaration
alternatives, including both conditional `MPAMIDR_EL1.HAS_BW_CTRL`
declarations, with zero unresolved or ambiguous occurrences. The overall TCTI
repository-wide audit remains blocked by later applicability, proof, cohort, and
runtime work.

## [2026-07-27] implement | Materialize Linux-visible proof ownership

The TCTI proof registry now materializes 6,364 source-bound Linux-proof rows:
all 4,350 direct leaves and 2,014 retained system-accessor variants. Every row
is either owned by source-and-build-bound Linux kselftest provenance or carries
a typed `not_applicable` reason. The completion audit rejects missing,
duplicate, stale, malformed, ambiguous, invalid-provenance, and non-kselftest
substitution rows. This records ownership only; executed proof remains zero.

## [2026-07-27] implement | Bind canonical artifacts to three-source provenance

The immutable OrlixTCTI artifact manifest is now V3. It binds every artifact row
to the pinned architecture, build, release, schema, timestamp, exact
Instructions, Features, and Registers byte lengths and digests, plus a computed
three-source reconciliation identity. Verification rejects field or row
tampering. Normal kernel, KUnit, kselftest, product, and audit graphs remain
isolated from Arm JSON inputs; canonical publication completed through Make.

## [2026-07-28] decide | Keep Arm shared ASL external

ADR 0031 records the official 2026-06 Arm XML and shared-ASL identities while
keeping their bodies external and non-redistributed because no applicable
authorization is verified. The 4,350-leaf AARCHMRS target remains unchanged;
independently authored OrlixTCTI behavior and source-bound production-path
KUnit and Linux kselftest evidence now own semantic completion. OpenMinis and
other implementations remain non-authoritative implementation references.

## [2026-07-28] implement | Publish complete external semantic provenance

The canonical OrlixTCTI artifact now classifies external semantic provenance
for all 4,350 direct AARCHMRS leaves. 4,332 rows bind exact matching-release
DDI0602 locators and digests; the remaining 18 bind exact AARCHMRS operation
locators and digests whose official operation is `// Not specified`. The audit
rejects missing, stale, incompatible, malformed, dangling, or ambiguous
provenance. Those 18 rows remain semantic blockers, while valid provenance
grants no implementation, proof, runtime-capability, or readiness credit. A
pinned OpenMinis AArch64 gadget map records reusable algorithms, edge cases,
test ideas, and rejected iOS-incompatible patterns for independent Orlix-owned
implementation without copying or adopting the external runtime.

## [2026-07-28] implement | Type the canonical AArch64 alias graph

Instruction artifact V4 preserves all 4,350 direct leaves plus the 292
source-declared instruction aliases and 171 reachable operation aliases as
supplemental graph rows. Every edge retains its declared and canonical concrete
operation target, source identity, typed relation, serialized predicate, and
predicate digest. Instruction aliases are source-conditioned assembler-only
edges; operation aliases are semantic redirects with an explicit unconditional
predicate. Encoding and decode kinds remain reserved because the pinned source
declares no such duplicate rows. Graph mutations fail closed and aliases grant
no classification, implementation, execution, proof, or runtime credit.

## [2026-07-28] implement | Finalize native proof ingestion contract

The TCTI proof registry now accepts execution credit only through opaque,
single-use result capabilities. Native records can be exported only after one
production `orlix_tcti_resume_user()` path, exact source binding, and a matching
internally captured RESULT or GPR observation. Caller-supplied memory, SIMD,
SVE, SME, fault, atomicity, and ordering witnesses remain ineligible. The
registry validates the exact source, condition, classification, owner, KUnit
suite and case, source and build provenance, kernel identity, and obligation;
replay and mismatch cases fail closed. Kselftest results and the ledger are also
opaque, and production has no kselftest PASS constructor until its owning Linux
execution issue supplies one. Canonical execution counts therefore remain zero.

## [2026-07-28] implement | Bind AARCHMRS operational notes to typed proof obligations

Corrected the A64 inventory importer to read `operational_note` from each
`Instruction.Instruction` leaf instead of the unrelated top-level operation
objects. Instruction artifact V5 now preserves exact source-bound note rows and
rejects malformed Arm `Text`, stale identities, duplicate or ambiguous proof
mappings, and unknown proof or native-case owners. The pinned source contains
4,350 absent notes and zero non-empty behavior obligations, so the generated
artifact publishes zero rows without a sentinel. Static mapping grants no
execution credit; the completion audit remains expected-red with zero executed
Linux proof rows. Host and direct KUnit gates pass. App-hosted KUnit remains
unrun because Xcode cannot resolve the configured simulator.

## [2026-07-28] guide | Remove machine-specific Xcode recovery policy

Removed the machine-specific external-storage recovery procedure from the
repository agent rules. Orlix verification now relies on repository-owned Make
targets and reports unavailable app-hosted evidence directly instead of
requiring a machine-local recovery helper.
