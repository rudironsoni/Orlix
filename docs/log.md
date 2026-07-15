---
type: meta
tags:
  - documentation
  - history
updated: 2026-07-15
---
# Orlix Knowledge Log

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
