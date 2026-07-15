---
name: orlix-docs-lint
description: Validate the Orlix documentation ontology, generated index, frontmatter, typed links, routes, and stale references. Use before shipping any Orlix knowledge change.
---

# Orlix Docs Lint

Use this skill as the mechanical gate for every change to the Orlix knowledge graph.

## Commands

- Regenerate the index: `rtk proxy python3 .agents/skills/orlix-docs-lint/scripts/build_index.py docs`
- Check index drift: `rtk proxy python3 .agents/skills/orlix-docs-lint/scripts/build_index.py --check docs`
- Run the complete lint gate: `rtk proxy python3 .agents/skills/orlix-docs-lint/scripts/wiki_link_check.py docs`

The gate must report `0 problems`. Fix canonical pages before changing the checker. A full review also checks duplicated facts, stale copied status, missing provenance, vague object types, and relationships that answer no domain question.

