---
name: orlix-codex-handoff
description: Use for long Orlix sessions, compaction risk, or a request to continue work from conversation history.
---
# Orlix Codex Handoff

Persist compaction and long-session continuation in `Build/AgentHarness/agent/continuation.json` through the PreCompact hook. Restore from that compact state and the active task envelope without rereading every active work page. Create a human-readable handoff in the operating-system temporary directory only when the user requests it. Repository-local chronological handoff files are forbidden.

Continuation state contains the selected task, current hypothesis, plan state, changed files, checks, evidence identities, failures, next gate, and unresolved decisions. Reference existing pages and reports instead of copying durable ontology facts, secrets, prompts, or logs.

If the handoff reveals durable missing knowledge, update the canonical ontology page through `orlix-docs-ingest` as a separate change and record that operation in `docs/log.md`.
