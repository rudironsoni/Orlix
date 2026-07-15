---
type: meta
tags:
  - documentation
  - ontology
updated: 2026-07-15
---
# Orlix Knowledge Base

This directory is the canonical Orlix knowledge graph. It models the product, software components, architecture decisions, epics, stories, tasks, product capabilities, reusable concepts, and source provenance as typed Markdown pages.

Start with [the generated index](index.md), then follow typed links to the relevant canonical pages. Read [the ontology](ontology.md) before changing the schema and [the maintenance protocol](AGENTS.md) before changing knowledge.

## Structure

```text
objects/    products, components, decisions, epics, stories, tasks, capabilities
concepts/  reusable architecture, ownership, proof, and workflow knowledge
sources/   faithful summaries and machine-readable source material
```

`index.md` is generated. `log.md` records knowledge-base operations. Product build and runtime evidence remains in the repository's structured build and report artifacts.
