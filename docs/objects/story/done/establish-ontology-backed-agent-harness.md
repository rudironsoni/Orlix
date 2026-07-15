---
type: story
tags:
  - story
  - agent-harness
updated: 2026-07-15
status: done
summary: "Use the typed documentation graph and structured reports as the agent harness source of truth."
story_of:
  - "[Orlix agent harness](../../epic/done/orlix-agent-harness.md)"
has_task:
  - "[Migrate durable knowledge into the ontology](../../task/done/migrate-durable-knowledge-into-ontology.md)"
  - "[Route volatile execution state to structured reports](../../task/done/route-execution-state-to-structured-reports.md)"
---

# Establish ontology-backed agent harness

As an Orlix contributor, I want agents to load durable knowledge from typed pages and current execution facts from structured reports so planning and implementation use one maintained source of truth.

The story is complete because the ontology, generated index, maintenance skills, lifecycle hooks, and structured `Build/AgentHarness/` reports now define that split.
