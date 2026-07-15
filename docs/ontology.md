---
type: meta
tags:
  - documentation
  - ontology
updated: 2026-07-15
---

# Orlix Ontology

The Orlix knowledge base adapts the [ontology-brain model](https://github.com/rudironsoni/ontology-brain) to a repository-local Markdown graph. Object types are folders, objects are pages, properties are frontmatter, relationships are typed links, reusable logic is stored in concepts, and source provenance is stored in source pages.

## Object Types

| Type | Meaning | Status vocabulary |
| --- | --- | --- |
| `product` | A delivered product with a stable identity | `active`, `retired` |
| `software-component` | A maintained software component with an ownership boundary | `active`, `retired` |
| `architecture-decision` | A durable architectural decision | `accepted`, `superseded` |
| `epic` | A durable outcome containing one or more stories | `todo`, `doing`, `done` |
| `story` | A user-valued increment belonging to exactly one epic and containing one or more tasks | `todo`, `doing`, `done` |
| `task` | An executable work unit belonging to exactly one story | `todo`, `doing`, `done` |
| `product-capability` | A capability with an independent product lifecycle | `implemented`, `partial`, `proposed`, `retired` |
| `concept` | Reusable architecture, terminology, method, or policy | optional |
| `source` | A faithful source summary or machine-readable source companion | `current`, `superseded` |

## Work Hierarchy

Work objects use a strict hierarchy:

```text
epic 1 -> N stories
story 1 -> N tasks
```

Each work type has `todo/`, `doing/`, and `done/` status folders. A page's `status` must match its folder. Moving a page between status folders is the lifecycle transition, and every inbound and mirrored relationship must be updated in the same change.

An epic owns the durable outcome and acceptance boundary. A story owns a coherent user-valued increment. A task owns a bounded executable unit. Exact commands, results, failures, evidence identities, and generated next-work selections remain in structured reports under `Build/AgentHarness/`.

## Properties

Every non-meta page requires:

- `type`: one object type, `concept`, or `source`.
- `tags`: searchable labels.
- `updated`: the last meaningful update date in `YYYY-MM-DD` form.

Optional shared properties are `aliases`, `status`, `external_id`, `summary`, and `sources`. `summary` is the stable one-line catalog hook. `sources` contains durable external URLs.

## Link Types

Typed links are flat top-level frontmatter lists whose values are quoted relative Markdown links.

| Link | Inverse | Meaning |
| --- | --- | --- |
| `has_story` | `story_of` | An epic contains a story |
| `story_of` | `has_story` | A story belongs to exactly one epic |
| `has_task` | `task_of` | A story contains a task |
| `task_of` | `has_task` | A task belongs to exactly one story |
| `part_of` | `has_part` | Structural containment outside the work hierarchy |
| `has_part` | `part_of` | Structural child outside the work hierarchy |
| `depends_on` | `blocks` | Delivery dependency |
| `blocks` | `depends_on` | Inverse delivery dependency |
| `targets` | none | The subject acts on or changes the target |
| `owned_by` | `owns` | Responsibility assignment |
| `owns` | `owned_by` | Responsibility held by the subject |
| `applies` | none | The subject uses a concept or method |
| `derived_from` | none | The page is grounded in a source page |
| `supersedes` | `superseded_by` | Complete replacement |
| `amends` | `amended_by` | Refinement that leaves the earlier decision valid |
| `relates_to` | none | Meaningful association without a sharper relationship |

`has_story`/`story_of`, `has_task`/`task_of`, `supersedes`/`superseded_by`, and `amends`/`amended_by` relationships must be mirrored. Use `targets` only when the source acts on or changes the target.

Markdown links are relative to the linking page. They may escape `docs/` to link repository source files, tests, scripts, or structured evidence, but they must remain inside the repository. Absolute local paths and `file:` URLs are forbidden. Typed frontmatter relationships target ontology pages.

## Normalization

Every fact has one canonical owner. Other pages link to that owner. Relationships that own dates, status, evidence, or lifecycle become pages rather than overloaded link values. `index.md` is derived and is never edited by hand.

Current runtime, release, and gate state is not a documentation property. Query the owning structured report or generated task artifact. Architecture acceptance boundaries remain in this graph.
