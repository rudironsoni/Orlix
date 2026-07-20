---
type: meta
tags:
  - documentation
  - history
updated: 2026-07-20
---
# Orlix Knowledge Log

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
