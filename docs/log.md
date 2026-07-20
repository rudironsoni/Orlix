---
type: meta
tags:
  - documentation
  - history
updated: 2026-07-19
---
# Orlix Knowledge Log

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
