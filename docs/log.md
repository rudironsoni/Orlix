---
type: meta
tags:
  - documentation
  - history
updated: 2026-07-21
---
# Orlix Knowledge Log

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

## [2026-07-20] guide | Record external Xcode mount recovery

Added durable agent guidance for simulator and Xcode recovery on the externally backed development environment. The procedure keeps Apple paths standard, uses `xcode-offload` for simulator lifecycle and mount restoration, distinguishes stale DerivedData reads from stale DeviceSet installation, remounts only the owning sparsebundle after stopping its holders, and requires strict doctor plus owning read or device verification before rebuilding.

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
