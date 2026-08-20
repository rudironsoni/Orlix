---
type: source
tags:
  - orlix-tcti
  - aarch64
  - provenance
  - implementation-reference
updated: 2026-08-20
status: current
summary: "Pinned OpenMinis AArch64 gadget patterns mapped to independent OrlixTCTI implementation work."
sources:
  - "https://github.com/OpenMinis/ish-arm64/tree/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64"
  - "https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/LICENSE.md"
  - "https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/LICENSE.IOS"
---

# OpenMinis AArch64 gadget implementation reference

## Boundary

This page records implementation patterns from
[`OpenMinis/ish-arm64`](https://github.com/OpenMinis/ish-arm64) that can shorten
independent OrlixTCTI work. OpenMinis is a non-authoritative implementation
reference. The pinned Arm DDI0602/AARCHMRS target remains the authority for
instruction semantics, feature conditions, exceptions, allocation state, and
proof expectations.

No OpenMinis source body is copied or mechanically translated into Orlix. An
Orlix family owner must derive expected behavior from the pinned Arm source,
write an Orlix-owned implementation in the existing Linux-hosted TCTI
architecture, and prove it through the production gadget path. OpenMinis can
suggest decomposition, edge cases, and tests, but it cannot earn semantic or
completion credit.

## Pinned source identity

- Repository: [`OpenMinis/ish-arm64`](https://github.com/OpenMinis/ish-arm64)
- Default branch at review: `master`
- Commit: [`de124dd66124a15239cea1465164f74980ada245`](https://github.com/OpenMinis/ish-arm64/commit/de124dd66124a15239cea1465164f74980ada245)
- Commit date: `2026-07-25T20:35:39+08:00`
- GitHub reports this repository as a fork of
  [`ish-app/ish`](https://github.com/ish-app/ish).

GitHub's path-filtered commit history contains 36 commits through the reviewed
snapshot. The gadget directory first appears in
[`6d729c6908ac52cea97741792f4f4029d92e9f5c`](https://github.com/OpenMinis/ish-arm64/commit/6d729c6908ac52cea97741792f4f4029d92e9f5c)
on `2026-02-07`; its newest path-changing commit before the snapshot is
[`6dc607511210dee506fce9d171bdcad2be135968`](https://github.com/OpenMinis/ish-arm64/commit/6dc607511210dee506fce9d171bdcad2be135968)
on `2026-07-11`. The repository commit pin, not either history endpoint, owns
the exact reviewed bytes.

The reviewed bytes are pinned independently of the moving branch:

| File | Bytes | Git blob | SHA-256 |
| --- | ---: | --- | --- |
| [`bits.S`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64/bits.S) | 2,656 | `fdc8f220a2de20416ece25da106e7fcbdd05ba12` | `f7b3e532d4e80880097edda8b520a7304d2ca6c65492b301c23d17be067d5814` |
| [`control.S`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64/control.S) | 39,002 | `9e375d450c045f42a0598a1a947e8873ea8c6a0d` | `d6226dc6a3e7a4404ff2518c649420bb5398558e1e254ed86a6351981b458bb0` |
| [`crypto.S`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64/crypto.S) | 19,010 | `09e3022c3664876946b4aebe596a3aeecb2c2566` | `befd17f2c7f318af3ebe76b320e2868be14d49184b4238d2bac1e344389797a5` |
| [`entry.S`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64/entry.S) | 17,316 | `acdbbfb9dcf96e94656ee275ee8f85ea3b74579c` | `53a4f8dc4183321fe19f3158b0dd1d6dd22cdb3468b62a475d2e8812787a7476` |
| [`gadgets.h`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64/gadgets.h) | 9,998 | `11b44a6f70b2f9a8d9cd758f004cc10017ed927c` | `a8715a952af631d9198fd86975f0dbc5ff15b9eb44241522df92a796751459eb` |
| [`math.S`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64/math.S) | 375,320 | `a8042465d3bcfb3c73e043448e1cb07bdd4b6197` | `adc37bc93bb2030e2e6803314ea64a3112fbf3ba3abc1a89431b00c9233e5e17` |
| [`memory.S`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/asbestos/guest-arm64/gadgets-aarch64/memory.S) | 94,328 | `03e28d19b43585be4b9b8c4f838bfef47fb76c0b` | `664f4079aaf5d74ccd91b1a598be328b99e1b570558f356de84c2ccac56d4779` |

The seven files contain 532 textual `.gadget` definitions or macro templates:
30 in `bits.S`, 28 in `control.S`, 26 in `crypto.S`, 7 in `entry.S`, 320 in
`math.S`, and 121 in `memory.S`. Macro expansion creates additional named
condition and vector variants.

## Orlix execution architecture to preserve

Orlix already has the correct iOS-compatible ownership shape:

1. [`decode_aarch64.c`](../../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/decode_aarch64.c)
   decodes a guest instruction into `struct orlix_tcti_decoded_instruction`.
2. [`gadget_program.c`](../../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/gadget_program.c)
   lowers decoded records into a data-only program that points only at fixed,
   already executable Orlix functions. No guest-derived executable code is
   created.
3. [`switch_debug.c`](../../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/switch_debug.c)
   contains the current Orlix-owned semantic helpers and debug-switch oracle.
   A family is production-complete only when lowering selects its fixed
   production gadget rather than depending on the debug switch.
4. [`block_cache.c`](../../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/block_cache.c)
   caches immutable data programs by `mm`, guest PC, and code generation.
5. [`tlb.c`](../../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tlb.c)
   keeps Linux page ownership, access permissions, page references, and
   translation-generation validation inside `arch/orlix`.
6. [`engine.c`](../../../OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/engine.c)
   owns structured exits back to Linux. Syscalls, faults, signals, scheduling,
   and VM policy remain Linux behavior.

The direct OpenMinis analogue worth retaining is a fixed gadget plus a data
record. Orlix should specialize `orlix_tcti_lower_decoded_instruction()` by
decode class, append a copied decoded or compact family payload, and dispatch
only to fixed Orlix gadget functions. It must not adopt OpenMinis CPU-state,
TLB, signal, block, syscall, or host-runtime ownership.

## File-to-Orlix implementation map

| OpenMinis source | Useful symbols and families | Orlix owner and concrete use | Do not carry over |
| --- | --- | --- | --- |
| `gadgets.h` | `.gadget`, `gret`, `read_prep`, `write_prep`, miss/cross-page tails, guest-register access, NZCV save/restore, C-call save/restore | Preserve the fixed-function plus data-record shape in `gadget_program.[ch]`. Reuse the TLB index and generation-validation idea through `orlix_tcti_tlb_index()`, `orlix_tcti_tlb_lookup_page()`, and `orlix_tcti_translation_generation_stable()`. Centralize contextual register reads through the existing `orlix_tcti_read_gpr_or_zero()` and `orlix_tcti_read_gpr_or_sp()` helpers. | Raw writable next-function pointers, a hard-coded host register ABI, direct `br` to unvalidated data, fixed 48-bit address masks, private MMU policy, and signal recovery. |
| `entry.S` | `fiber_enter`, `fiber_ret_chain`, `fiber_ret`, `fiber_exit`, read/write misses, cross-page load/store, `set_pc`, `interrupt`, `exit`, `syscall`, `undefined`, crash trampoline | Use only the state-transition decomposition: observe entry PC/instruction, execute one fixed gadget, return a typed status, and retry Linux faults through `engine.c`. Map syscall, breakpoint, unsupported, yield, user fault, and alignment fault to existing `ORLIX_TCTI_EXIT_*` handling. | Fiber ABI, `INT_*` private interrupts, userspace syscall emulation, SIGSEGV-as-memory-model, crash trampolines, fake-IP tagging, and direct host-pointer recovery. |
| `bits.S` | `lsl*`, `lsr*`, `asr*`, `ror*`, extensions, `clz*`, `cls*`, `rbit*`, `rev*` | Cross-check operand-width and zero-extension cases for `orlix_tcti_execute_data_processing_1source()` and `orlix_tcti_execute_data_processing_2source()`. | Its `bfm64`, `sbfm64`, `ubfm64`, and `extr64` bodies are explicit stubs. They are negative evidence, not reusable algorithms. Orlix uses `orlix_tcti_execute_bitfield()` and `orlix_tcti_execute_extract()` derived from Arm semantics. |
| `control.S` | `branch`, `branch_link`, `branch_reg`, `branch_link_reg`, `ret`, `cbz*`, `cbnz*`, `tbz`, `tbnz`, `bcond`, `csel`, `csinc`, `csinv`, `csneg`, per-condition and compare/branch fusion templates | Reuse condition partitioning and target/fallthrough test shapes in the Orlix branch cases, `orlix_tcti_condition_passed()`, `orlix_tcti_execute_conditional_compare()`, and `orlix_tcti_execute_conditional_select()`. Consider fusion only after each constituent instruction has production proof and only as an equivalent lowering optimization. | Fake-IP bit 63, unvalidated direct block chaining, writable return-cache continuations, 48-bit target masking, whole-function `prebuilt_entry`, and fusion as a substitute for individual leaf proof. |
| `crypto.S` | `aese`, `aesd`, `aesmc`, `aesimc`, SHA-1, SHA-256, SHA-512, `eor3`, `sm3partw1`, `sm4e`, `pmull`, `tbl`, `tbx`; two fixed-width `sve_*` gadgets | Cross-check lane selection, destructive destination use, upper-bit clearing, and source-half selection for `orlix_tcti_execute_simd_vector_arithmetic()` and `orlix_tcti_execute_simd_table_lookup()`. Existing Orlix AES GF/substitution/row/column helpers remain independently authored. | Native `.inst` execution without an Orlix-controlled host-feature gate, helper results as authority, and the 128-bit-only SVE model. The two `sve_eor_d` and `sve_xar_d` gadgets cannot model architectural scalable vectors or predicates. |
| `math.S` | Scalar add/sub/carry/logical/shift/divide/multiply/bitfield, system-register access, AdvSIMD integer/FP/permute/conversion, and scalar FP/conversion families | Use its family decomposition to select fixed Orlix gadgets around existing helpers: add/sub helpers, logical helpers, bitfield/extract, multiply/data-processing, SIMD modified-immediate/element/table/logical/arithmetic/compare/reduction, and scalar FP helpers. Use macro-generated variants only as a checklist for width, lane shape, high-half, accumulate, and rounding cases. | Host FP or SIMD instructions as semantic authority; unchecked host FPCR/FPSR coupling; macro expansion as coverage evidence; native instruction use when host and guest feature sets differ; OpenMinis-specific peephole fusion. |
| `memory.S` | Effective-address paths, scalar/pair/SIMD loads and stores, pre/post-index writeback, cross-page tails, LSE RMW/CAS, exclusive load/store pairs, true CASP, DMB, IC IVAU | Cross-check address/writeback partitions in `orlix_tcti_execute_load_literal()`, load/store immediate/register/pair helpers, SIMD structure helpers, exclusive and LSE helpers. Retain Orlix Linux-page access through `tlb.c`, `orlix_tcti_read_user_data()`, and `orlix_tcti_write_user_data()`. Retain true 128-bit atomicity and translation-generation checks. | A private TLB/MMU, plain host loads/stores for guest atomics, masking guest addresses to 48 bits, conflating DMB/DSB/ISB, treating IC IVAU as an unconditional host cache operation, or applying writeback before a faulting access is architecturally committed. |

## Family issue map

Every issue from `#123` through `#154` is listed. "None" means the reviewed
OpenMinis files do not provide a usable family pattern; the issue still follows
the pinned Arm source and Orlix proof contract.

| Issue | OpenMinis leverage | Orlix implementation target and first proof vectors |
| --- | --- | --- |
| [#123](https://github.com/rudironsoni/Orlix/issues/123) AdvSIMD crypto | `crypto.S`: AES, SHA-1/2/512, EOR3, SM3, SM4, PMULL. `math.S`: PMUL vector shape. | Specialize lowering to the SIMD arithmetic gadget. Exercise destructive destinations, source/destination aliasing, PMULL versus PMULL2 source halves, all-zero/all-one polynomial operands, AES round ordering, and upper-bit preservation. Extend `orlix_tcti_crypto_*` and PMULL production tests. |
| [#124](https://github.com/rudironsoni/Orlix/issues/124) AdvSIMD FP | `math.S`: FP unary, three-same, pairwise/reduction, compare-zero, rounding, conversion templates. | Route SIMD arithmetic/reduction/compare through fixed gadgets. Cover `+0/-0`, quiet/signaling NaNs, infinities, subnormals, lane widths, Q=0/Q=1, FPCR rounding modes, FPSR effects, and inactive upper lanes. |
| [#125](https://github.com/rudironsoni/Orlix/issues/125) AdvSIMD integer | `math.S`: widening/narrowing, saturating, halving/rounding, shifts, min/max, pairwise, reductions. | Route SIMD arithmetic/compare/logical/modified-immediate families. Test lane endpoints, signed versus unsigned saturation, QC updates, high-half forms, narrowing destination preservation, variable shift counts outside element width, and aliasing. |
| [#126](https://github.com/rudironsoni/Orlix/issues/126) AdvSIMD load/store | `memory.S`: single and multiple structures, replicate, interleaved, scalar/Q loads and stores. | Orlix owns production decode, execute, and typed `REGISTERS`, `MEMORY`, `PC`, and `FAULTS` observations for the 152 `ADVSIMD_LOAD_STORE` leaves through Linux TLB and uaccess. STL1 and LDAP1 are transfer-only. Ordinary SIMD LDR/STR stay in [#134](https://github.com/rudironsoni/Orlix/issues/134). Exclusive and LSE stay in [#128](https://github.com/rudironsoni/Orlix/issues/128). |
| [#127](https://github.com/rudironsoni/Orlix/issues/127) AdvSIMD permute/move | `math.S`: DUP, INS, UMOV/SMOV, XTN, UZP, TRN, ZIP, EXT, REV. `crypto.S`: TBL/TBX. | Route element-move and table-lookup gadgets. Test lane index endpoints, Q=0 upper-half rules, table lists wrapping V31, out-of-range TBL zeroing versus TBX preservation, signed extraction, and overlapping source/destination registers. |
| [#128](https://github.com/rudironsoni/Orlix/issues/128) atomics/exclusives/ordered memory | `memory.S`: `atomic_rmw`, `atomic_cas`, `ldxr`, `stxr`, pair exclusives, `atomic_casp*`, `dmb`. | Orlix owns production decode, execute, and typed `REGISTERS`, `MEMORY`, `PC`, `ATOMICITY`, `ORDERING`, and `FAULTS` observations for the 498 `BASE_ATOMICS` leaves through Linux uaccess. Pair RCW is UNDEFINED in the product scalar RCW domain (`FEAT_D128=n`). Exclusive-monitor mismatch, CAS/RMW old-value, and CASP whole-pair extras (including 32-bit pair packing) stay on the same suite. `dmb`/`dsb`/`isb`/`clrex` stay in [#139](https://github.com/rudironsoni/Orlix/issues/139). Ordinary load/store stays in [#134](https://github.com/rudironsoni/Orlix/issues/134). `HWCAP_ATOMICS` is not advertised. |
| [#129](https://github.com/rudironsoni/Orlix/issues/129) bitfield/extract/unary | `math.S`: UBFM/SBFM/BFM, EXTR, CLZ/CLS/RBIT/REV variants. `bits.S` contributes only width test shapes; its bitfield bodies are stubs. | Route bitfield, extract, and DP1 gadgets. Test `immr <= imms` and wrap cases, 32/64-bit masks, sign-fill, destination merge, EXTR source concatenation and aliasing, zero inputs, and W-register upper-zeroing. |
| [#130](https://github.com/rudironsoni/Orlix/issues/130) branch/control | `control.S`: direct/register branches, BL/BLR/RET, CBZ/CBNZ, TBZ/TBNZ. | Orlix owns production decode, execute, and typed `REGISTERS` and `PC` observations for the five EL0 `BASE_CONTROL_FLOW` leaves B, BL, BR, BLR, and RET. `FLAGS`, `MEMORY`, `ATOMICITY`, and `ORDERING` are not applicable for those EL0 leaves. ERET, ERETAA, ERETAB, DRPS, and TEXIT are classified `NON_EL0` and prove EL0 rejection. TEXIT official DDI0602 semantics are unspecified. Reserved encodings stay decode `UNSUPPORTED`. CBZ/CBNZ/TBZ/TBNZ stay in [#131](https://github.com/rudironsoni/Orlix/issues/131). SVC/BRK stay in [#132](https://github.com/rudironsoni/Orlix/issues/132). Pointer-auth branches stay in [#137](https://github.com/rudironsoni/Orlix/issues/137). BTI stays in [#139](https://github.com/rudironsoni/Orlix/issues/139). `FEAT_TEV` is not advertised. |
| [#131](https://github.com/rudironsoni/Orlix/issues/131) conditional compare/select/branch | `control.S`: condition tables, CSEL/CSINC/CSINV/CSNEG, fused compare/branch. `math.S`: CCMP/CCMN immediate/register. | Route conditional helpers. Exhaust all 16 condition encodings across representative NZCV states; verify false CCMP/CCMN installs immediate NZCV, true paths calculate carry/overflow at 32/64 bits, and aliases preserve W upper-zeroing. Fusion remains a later equivalence optimization. |
| [#132](https://github.com/rudironsoni/Orlix/issues/132) exceptions/undefined | `entry.S`: typed interrupt/undefined exits. `control.S`: SVC. | Keep Orlix structured exits through `engine.c`. Test SVC handoff without userspace emulation, BRK versus HLT distinction, reserved/unallocated encodings, PC advancement or retention, fault address/access class, and Linux SIGILL/SIGTRAP integration. |
| [#133](https://github.com/rudironsoni/Orlix/issues/133) add/subtract | `math.S`: immediate, shifted, extended, carry, flag-setting and SP-specialized variants. | Orlix owns production decode, execute, and typed `REGISTERS`, `PC`, and `FLAGS` observations for the 34 `BASE_ADD_SUBTRACT` leaves. `MEMORY`, `ATOMICITY`, and `ORDERING` are not applicable. ADDPT and SUBPT keep PAC tag bits [63:56] and do 56-bit pointer arithmetic. Reserved encodings stay decode `UNSUPPORTED`. Pointer-auth keys stay in [#137](https://github.com/rudironsoni/Orlix/issues/137). SUBP stays in [#185](https://github.com/rudironsoni/Orlix/issues/185). `FEAT_CPA` is not advertised. |
| [#134](https://github.com/rudironsoni/Orlix/issues/134) load/store/prefetch | `memory.S`: literal, immediate, register-offset, signed load, scalar/pair, pre/post-index, fast and cross-page paths. | Orlix owns production decode, execute, and typed `REGISTERS`, `MEMORY`, `PC`, and `FAULTS` observations for the 209 `BASE_LOAD_STORE` leaves through Linux TLB and uaccess. Later families copy `orlix_tcti_memory_proof.h` and the ordinal-plus-obligation capture walk. Prefetch remains a no-fault hint. COW and self-modifying-code invalidation stay in the mapping-invalidation suite. Exclusive and LSE stay in [#128](https://github.com/rudironsoni/Orlix/issues/128). |
| [#135](https://github.com/rudironsoni/Orlix/issues/135) memory tagging | None. The reviewed memory code masks addresses and has no architectural MTE tag/check state. | Implement independently from Arm semantics. Do not reuse the 48-bit mask because it destroys tag and fault information. Prove allocation tags, logical tags, checked/unchecked accesses, synchronous/asynchronous fault modes, and Linux-visible controls. |
| [#136](https://github.com/rudironsoni/Orlix/issues/136) multiply/divide/ternary | `math.S`: MADD/MSUB, signed/unsigned long forms, high multiply, UDIV/SDIV. | Route multiply/DP2 gadgets. Test zero divisors, signed minimum divided by `-1`, W upper-zeroing, long-form source extension, high-half products, accumulator aliasing, and XZR operands/destinations. |
| [#137](https://github.com/rudironsoni/Orlix/issues/137) pointer authentication | None. The reviewed gadgets do not implement PAUTH, BTI, or GCS semantics. | Keep the existing PAUTH/BTI/GCS obligation ledger and implement from pinned Arm semantics. Do not infer keys, modifiers, failure behavior, branch authentication, or landing-pad policy from ordinary branch gadgets. |
| [#138](https://github.com/rudironsoni/Orlix/issues/138) residual named classes | `math.S`: move-wide, ADR, CRC and limited system-register shapes. `entry.S`: explicit unsupported exit shape. | Use the inventory row's owning decode class, not a catch-all gadget. First vectors include MOVK merge, ADR/ADRP page base, CRC polynomial/width, legal aliases, reserved encodings, and deterministic rejection for non-executable EL0 leaves. |
| [#139](https://github.com/rudironsoni/Orlix/issues/139) system/hints/barriers | `memory.S`: DMB and IC IVAU. `math.S`: TPIDR, NZCV, FPCR/FPSR and generic system-register shapes. | Route each reconciled accessor or barrier class to a fixed gadget. Test EL0 permissions, read-as-zero/write-ignore dispositions, counter behavior, DMB/DSB/ISB option domains, nXS distinction, cache-maintenance faults and code invalidation, and unsupported privileged leaves. |
| [#140](https://github.com/rudironsoni/Orlix/issues/140) empty SME crypto | None, correctly non-applicable. | Issue is closed as an empty cohort. Do not map AdvSIMD crypto or fixed-width SVE gadgets into SME. |
| [#141](https://github.com/rudironsoni/Orlix/issues/141) SME FP | None. | Implement from Arm semantics with independent ZA/ZT0/streaming-mode state and feature gating. OpenMinis vector FP assumes fixed 128-bit AdvSIMD state. |
| [#142](https://github.com/rudironsoni/Orlix/issues/142) SME integer/logical/conversion | None. | Implement independent scalable/matrix state, predicate behavior, streaming mode transitions, and feature dependencies. |
| [#143](https://github.com/rudironsoni/Orlix/issues/143) SME load/store/prefetch | None. | Implement ZA slice addressing, predicates, alignment/fault ordering, writeback, and Linux memory access through Orlix TLB/uaccess. |
| [#144](https://github.com/rudironsoni/Orlix/issues/144) SME multiply/dot-product/matrix | None. | Implement independent ZA accumulation, element-size combinations, signedness, widening, and feature-conditioned variants. |
| [#145](https://github.com/rudironsoni/Orlix/issues/145) SME permute/move | None. | Implement independent ZA/ZT0 transfer and slice semantics. AdvSIMD lane templates do not define SME state. |
| [#146](https://github.com/rudironsoni/Orlix/issues/146) SME predicate/control | None. | Implement SMSTART/SMSTOP, streaming vector length, ZA enable state, predicates, and exception behavior from Arm semantics. |
| [#147](https://github.com/rudironsoni/Orlix/issues/147) SVE crypto | `crypto.S` has only `sve_eor_d` and `sve_xar_d`, explicitly modeled as 128-bit values. This is insufficient and semantically unsafe for SVE proof. | Implement scalable crypto operations through `sve_state.[ch]` with true VL, predicates, merging/zeroing, element sizes, destructive operands, and feature gates. Use OpenMinis only to remember EOR/XAR aliasing and immediate-boundary tests. |
| [#148](https://github.com/rudironsoni/Orlix/issues/148) SVE FP | None. | Implement scalable FP lanes, predicates, FPCR/FPSR behavior, NaNs, inactive lanes, and feature-conditioned element widths independently. |
| [#149](https://github.com/rudironsoni/Orlix/issues/149) SVE integer/logical/conversion | The two fixed-width `sve_*` gadgets provide only operation-name discovery, not a state model. | Extend `sve_decode.c`, `sve_state.c`, and fixed production gadgets beyond the current predicated integer binary slice. Test every supported VL, `/M` versus `/Z`, inactive lanes, overlapping registers, element sizes, and immediate boundaries. |
| [#150](https://github.com/rudironsoni/Orlix/issues/150) SVE load/store/prefetch | None. | Implement predicated contiguous/gather/scatter/structure accesses through Linux pages, including first-fault/non-fault state, inactive lanes, per-lane faults, tag/alignment behavior, and no speculative host access for inactive lanes. |
| [#151](https://github.com/rudironsoni/Orlix/issues/151) SVE multiply/dot-product | None. | Implement scalable widening, signedness, indexed elements, accumulation, and feature-conditioned dot-product forms independently. |
| [#152](https://github.com/rudironsoni/Orlix/issues/152) SVE permute/move | None. | Implement scalable ZIP/UZP/TRN/EXT/table/compact/splice/index behavior with predicate and VL boundaries; AdvSIMD templates are only test-shape inspiration. |
| [#153](https://github.com/rudironsoni/Orlix/issues/153) SVE predicate/control | None. | Implement predicate generation, tests, count/increment, break/first/next, FFR, and condition-flag effects from Arm semantics. |
| [#154](https://github.com/rudironsoni/Orlix/issues/154) scalar FP/conversion | `math.S`: scalar immediate/move, compare, conditional compare/select, unary/binary/ternary, fixed and integer conversions. | Route scalar FP gadgets around Orlix fixed-FP helpers. Cross product H/S/D, all rounding modes, fixed-point scale endpoints, signed/unsigned saturation, NaNs, infinities, subnormals, signed zero, signaling compares, FPCR controls, FPSR cumulative flags, and GPR/SIMD high-half moves. |

## Reusable algorithms and edge cases

These are patterns to re-derive and implement in Orlix, not code to copy.

### Width and register semantics

- Dispatch on architectural width before calculating flags. Every W-register
  write clears bits 63:32, including conditional false paths and failed-looking
  aliases that still write a result.
- Resolve register 31 at the instruction-family boundary. It means `SP` in
  specific address/add-sub forms and `XZR/WZR` in logical, compare, branch, and
  most data-processing forms.
- Preserve untouched SIMD bits exactly where Arm specifies partial writes.
  Conversely, scalar and narrowing forms that architecturally clear upper bits
  must do so even when source and destination alias.
- Treat destructive destinations as an explicit old-destination input. AES,
  accumulating arithmetic, BSL/BIT/BIF, insert, TBX, and several SME/SVE forms
  expose bugs if the destination is overwritten before all inputs are read.

### Conditions and flags

- A 16-entry condition truth table is a useful exhaustive test generator. AL
  and the reserved/always encoding must follow the pinned Arm pseudocode rather
  than a convenient host branch alias.
- Calculate guest `N`, `Z`, `C`, and `V` at the guest width. Do not rely on host
  flags surviving C code, helper calls, or unrelated gadget dispatch.
- Conditional compare has two state transitions: calculate flags when the
  condition passes, or install the encoded immediate NZCV when it fails. Test
  both for every condition family.

### Memory and atomicity

- Check page crossing before taking a fast host pointer. A two-page access must
  report the architecturally correct first failing byte and must not leak a
  partial store unless the Arm operation explicitly permits it.
- Validate `mm`, guest page, Linux permissions, retained page reference, and
  translation generation at use time. Orlix already has stronger ownership
  than OpenMinis and must keep it.
- Apply pre/post-index writeback only at the architectural commit point. Tests
  must distinguish address calculation, access fault, partial multi-register
  progress, and base/destination overlap constraints.
- Exclusive success requires a matching live monitor and an atomic comparison
  against the observed value. Clear or invalidate the monitor on every
  architecturally required path. Pair exclusives and CASP require one atomic
  whole-pair commit, never two scalar commits.
- Model acquire/release and barriers through the Arm ordering contract and
  Linux concurrency primitives. A single strongest host fence may hide missing
  option, scope, or completion semantics and cannot close proof by itself.

### Control flow and cache coherence

- Keep target and fallthrough explicit in the lowered data. Test signed branch
  displacement limits and PC-relative base selection before considering block
  chaining.
- Any future chaining must retain `mm` identity, code generation, refcount/RCU
  lifetime, and an allowlisted fixed gadget entry. A guest-controlled or stale
  writable pointer must never become an unchecked host branch target.
- IC IVAU and executable-page writes must invalidate every affected translated
  block before the new guest instruction bytes can execute. OpenMinis' explicit
  invalidation path is a useful scenario; Orlix's generation protocol remains
  authoritative.

### Vector, SVE, and SME state

- Generate test matrices from element width, vector length, register overlap,
  high-half selection, predicate mode, inactive-lane behavior, and feature
  condition. Operation-name coverage alone misses most state transitions.
- Never infer SVE or SME behavior from a 128-bit AdvSIMD implementation. SVE
  requires architectural VL and predicate state; SME adds streaming mode,
  streaming VL, ZA, and sometimes ZT0.
- Native host instructions are optional implementation tools only after
  runtime host-feature checks and an equivalent fallback. They are never the
  semantic oracle.

## Patterns rejected for Orlix

| OpenMinis pattern | Why Orlix rejects it |
| --- | --- |
| Writable stream entries used as raw `br` targets or chained block pointers | Orlix data programs must select only fixed trusted code, remain generation-authorized, and retain block lifetime. |
| Guest addresses masked to `0xffffffffffff` | This destroys architectural address, tag, and fault information and cannot support issue #135 correctly. |
| SIGSEGV crash recovery as normal guest memory handling | Orlix has Linux page ownership and structured user-fault exits; Darwin signal recovery cannot own Linux fault semantics. |
| Private TLB/MMU and host-pointer-minus-guest-address translation | `tlb.c` must retain Linux pages, Linux permissions, COW sensitivity, and translation generations. |
| `prebuilt_entry` whole-function replacement | It bypasses instruction-leaf classification and production-path proof and risks moving Linux/userspace behavior into a host helper. |
| Host native crypto/FP/SIMD instruction with no feature-equivalent fallback | iOS device features can differ from the guest target. Host execution cannot silently narrow the advertised architecture. |
| Fixed 128-bit SVE | It omits scalable VL, predicate state, inactive lanes, first-fault state, and SME interactions. |
| Peephole fusion used as implementation evidence | Fused blocks are performance work after both instructions have independent semantic and production-path proof. |
| Stub gadgets such as the `bits.S` bitfield/extract bodies | Presence of a symbol is not an implementation and cannot supply expected results. |
| OpenMinis helper output or workload success as oracle | Only pinned Arm semantics and Orlix source-bound proof establish correctness. |

## License and provenance constraint

GitHub reports `NOASSERTION` for the repository license metadata. The pinned
[`LICENSE.md`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/LICENSE.md)
states that iSH is GPLv3 and that contributions made after commit
`0e3a4144f93135c4fd618c8397d2cfd87194f69f` are additionally licensed under
GPLv2. The pinned
[`LICENSE.IOS`](https://github.com/OpenMinis/ish-arm64/blob/de124dd66124a15239cea1465164f74980ada245/LICENSE.IOS)
records the iSH copyright holders' App Store enforcement commitment, subject to
the remaining GPL obligations.

The reviewed `LICENSE.md` is 1,731 bytes, Git blob
`21ee4d545a192e8400a4baa16301491450bd827d`, SHA-256
`300e62ce6fafbee38b072ab7daf8d64bd57042d81e09d3b4c58af26bcabddab1`.
The reviewed `LICENSE.IOS` is 782 bytes, Git blob
`106cfd4667fac19391c49aebd1d4c96e04436d00`, SHA-256
`37708e19d5ea72c1491926ce6d33ae33ab746609b772a4f1b1657a809f835958`.

This page does not decide the license of an individual line or provide legal
advice. Orlix's accepted clean-room/reference-only rule is stricter: do not copy,
vendor, assemble, link, or mechanically translate these files. If source reuse
is ever proposed, it requires a separate file-level authorship and license
review, attribution, GPL compatibility analysis for the destination, and App
Store distribution analysis before any code change.

## Family-owner implementation sequence

1. Select the next applicable issue and its exact AARCHMRS leaf set.
2. Use the table above to collect edge-case shapes from the relevant OpenMinis
   symbols without using their result as expected behavior.
3. Derive semantics and exception behavior from the pinned Arm source.
4. Add or refine the Orlix decoder and independently authored semantic helper.
5. Add a fixed production gadget and make
   `orlix_tcti_lower_decoded_instruction()` select it with a copied data payload.
6. Add source-bound KUnit covering valid, reserved, feature-disabled, boundary,
   aliasing, fault, and state-transition cases through the production gadget
   program. Keep the debug switch only as a paired development oracle.
7. Add the owning Linux kselftest where behavior is Linux-visible, then rerun
   the target audit. OpenMinis reference coverage changes no proof counter.
