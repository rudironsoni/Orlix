---
name: orlix-codex-handoff
description: Use for long Orlix sessions, compaction risk, or a request to continue work from conversation history.
---
# Orlix Codex Handoff

Create ephemeral handoffs in the operating system temporary directory. The repository ontology contains durable knowledge, and structured reports contain current workflow state, so repository-local chronological handoff files are forbidden.

Include the repository path, branch, relevant commit, user objective, owning ontology pages, current structured report paths, files changed, exact checks run, failures and skips, unresolved decisions, and the next concrete actions. Reference existing pages, commits, and reports instead of copying them. Redact secrets and bulky logs.

If the handoff reveals durable missing knowledge, update the canonical ontology page through `orlix-docs-ingest` as a separate change and record that operation in `docs/log.md`.
