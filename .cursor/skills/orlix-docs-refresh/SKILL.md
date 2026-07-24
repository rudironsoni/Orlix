---
name: orlix-docs-refresh
description: Reconcile current Orlix source, reports, decisions, and multiple ontology pages without rewriting unchanged knowledge.
---
# Orlix Docs Refresh

1. Read `docs/AGENTS.md`, `docs/ontology.md`, `docs/index.md`, and recent `docs/log.md` entries.
2. Define the exact source set, object types, and date window.
3. Re-read changed raw sources and structured reports.
4. Update canonical pages only where durable facts changed.
5. Propagate relationships, regenerate the index, append the log when facts changed, and run `orlix-docs-lint`.

Do not refresh by rewriting every page. Keep unresolved claims labeled and keep current gate state in generated artifacts.
