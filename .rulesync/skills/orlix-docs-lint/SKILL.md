---
name: orlix-docs-lint
description: >-
  Validate the Orlix documentation ontology, generated index, frontmatter, typed
  links, routes, and stale references. Use before shipping any Orlix knowledge
  change.
targets:
  - '*'
---
# Orlix Docs Lint

Use this skill as the mechanical gate for every change to the Orlix knowledge graph.

## Commands

- Regenerate the index: `make docs-index`
- Run the complete lint gate: `make docs-check`
- List the current agent leaf frontier: `make agent-frontier`
- Compare mapped ontology work with GitHub: `make agent-graph-check`

The gate must report `0 problems`. Fix canonical pages before changing the checker. A full review also checks duplicated facts, stale copied status, missing provenance, vague object types, and relationships that answer no domain question.
