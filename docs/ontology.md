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
| `product` | A delivered product with stable identity | `active`, `retired` |
| `software-component` | A maintained software component with an ownership boundary | `active`, `retired` |
| `architecture-decision` | A durable architectural decision | `accepted`, `superseded` |
| `initiative` | A durable work objective with acceptance boundaries | `active`, `blocked`, `deferred`, `completed` |
| `product-capability` | A capability with independent product lifecycle | `implemented`, `partial`, `proposed`, `retired` |
| `concept` | Reusable architecture, terminology, method, or policy | optional |
| `source` | Faithful source summary or machine-readable source companion | `current`, `superseded` |

## Properties

Every non-meta page requires:

- `type`: one object type, `concept`, or `source`.
- `tags`: searchable labels.
- `updated`: the last meaningful update date in `YYYY-MM-DD` form.

Optional shared properties are `aliases`, `status`, `external_id`, `summary`, and `sources`. `summary` is a stable one-line catalog hook. `sources` contains durable external URLs.

## Link Types

Typed links are flat top-level frontmatter lists whose values are quoted relative Markdown links.

| Link | Inverse | Meaning |
| --- | --- | --- |
| `part_of` | `has_part` | hierarchy |
| `depends_on` | `blocks` | ordering or dependency |
| `targets` | none | an initiative or decision acts on the target |
| `owned_by` | `owns` | responsibility |
| `applies` | none | an object uses a concept or method |
| `derived_from` | none | a page is grounded in a source page |
| `supersedes` | `superseded_by` | complete replacement |
| `amends` | `amended_by` | refinement that leaves the earlier decision valid |
| `relates_to` | none | meaningful association without a sharper relationship |

`supersedes` and `amends` relationships must be mirrored. Use `targets` only when the source acts on or changes the target.

Markdown links are relative to the linking page. They may escape `docs/` to link repository source, tests, scripts, and structured evidence, but they must remain inside the repository. Absolute local paths and `file:` URLs are forbidden. Typed frontmatter relationships target ontology pages.

## Normalization

Every fact has one canonical owner. Other pages link to that owner. Relationships with their own dates, status, evidence, or lifecycle become pages rather than overloaded link values. `index.md` is derived and never edited by hand.

Current runtime, release, and gate state is not a documentation property. Query the owning structured report or generated task artifact. Architecture and acceptance boundaries remain in this graph.
