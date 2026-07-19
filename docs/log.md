---
type: meta
tags:
  - documentation
  - history
updated: 2026-07-19
---
# Orlix Knowledge Log

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
