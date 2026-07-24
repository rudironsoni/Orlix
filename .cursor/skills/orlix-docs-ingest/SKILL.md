---
name: orlix-docs-ingest
description: Add or update Orlix knowledge in the canonical docs ontology. Use for architecture, decision, epic, story, task, capability, terminology, provenance, and durable lesson changes.
---
# Orlix Docs Ingest

1. Read `docs/AGENTS.md`, `docs/ontology.md`, and the relevant canonical pages.
2. Identify the object, concept, or source that owns each fact.
3. Cross-check repository source, tests, reports, and current decisions.
4. Update only canonical owners and propagate changed relationships.
5. Use `python3 .agents/skills/orlix-docs-ingest/scripts/wiki_link_repair.py --dry-run` before a rename or move.
6. Regenerate `docs/index.md`, append `docs/log.md`, and run `orlix-docs-lint`.

Do not copy volatile build, simulator, device, gate, or release status into canonical pages. Preserve uncertainty labels until current evidence resolves them.
