---
type: meta
tags:
  - documentation
  - agents
updated: 2026-07-15
---
# Orlix Knowledge Maintenance Protocol

Read this file and [the ontology](ontology.md) before changing Orlix knowledge. The goal is one canonical home per fact, explicit relationships, current provenance, and small pages that answer a specific domain question.

## Layers

1. Repository source, tests, structured reports, upstream material, and external references are raw evidence.
2. `objects/`, `concepts/`, and `sources/` are the maintained semantic graph.
3. `ontology.md` defines the schema and `index.md` is its generated catalog.

## Page Placement

- `objects/product/`: delivered products with identity.
- `objects/software-component/`: maintained software components with ownership boundaries.
- `objects/architecture-decision/`: durable decisions with preserved ADR identifiers.
- `objects/epic/{todo,doing,done}/`: durable outcomes and acceptance boundaries.
- `objects/story/{todo,doing,done}/`: user-valued increments, each linked to exactly one epic.
- `objects/task/{todo,doing,done}/`: bounded executable work units, each linked to exactly one story.
- `objects/product-capability/`: user-visible or operational capabilities with independent lifecycle state.
- `concepts/`: reusable architecture, terminology, ownership, proof, and workflow knowledge.
- `sources/`: faithful source summaries and machine-readable inputs.

## Rules

- Store each fact once and link to its owner.
- Use Markdown links relative to the linking page. Escaped links to source files may leave `docs/` but must remain inside the repository; never use absolute local paths or `file:` URLs.
- Use absolute dates.
- Label unresolved claims `[INFERENCE]`, `[SPECULATION]`, or `[UNVERIFIED]`.
- Use `[CORRECTION]` when repairing an earlier unsupported claim.
- Keep volatile gate selection, build identity, simulator identity, and runtime status in structured report artifacts.
- Treat a capability status of `implemented` as source and focused-test presence only. It is not runtime or release proof.
- Use relative Markdown links within `docs/`. Use code formatting for repository paths outside `docs/`.
- Add a new link type only when the relationship recurs and `relates_to` would lose useful meaning.
- Keep `epic -> story -> task` relationships mirrored with `has_story`/`story_of` and `has_task`/`task_of`.
- Keep every work page in the folder matching its `todo`, `doing`, or `done` status.
- Put durable outcomes, acceptance boundaries, and work decomposition in epic, story, and task pages. Keep exact commands, results, failures, and selected next work in structured reports.
- Update both ends of mirrored hierarchy, `supersedes`, and `amends` relationships.
- Regenerate `index.md` after page changes and append a concise entry to `log.md` for meaningful knowledge changes.

## Workflows

- Use `orlix-docs-ingest` for new or changed knowledge.
- Use `orlix-docs-query` to answer from canonical pages.
- Use `orlix-docs-refresh` for multi-page reconciliation.
- Use `orlix-docs-lint` before shipping any knowledge change.
- Use `orlix-ontology` when changing object types, properties, relationships, or normalization rules.
