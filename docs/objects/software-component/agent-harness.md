---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-09-13
status: active
summary: "Repository-local rules, skills, hooks, and structured workflow state."
part_of:
  - "[Orlix](../product/orlix.md)"
owns:
  - "[Redesign the agentic harness](../epic/doing/redesign-agentic-harness.md)"
applies:
  - "[Agentic harness authority](../../concepts/agentic-harness-authority.md)"
---

# Agent harness

The ontology owns durable work meaning. `.rulesync/` owns portable configuration. RuleSync generates client files. `Tools/AgentHarness/` owns Orlix task context, task envelopes, lifecycle semantics, continuation state, proof checks, and reports. GitHub CI owns generated-output enforcement. `Build/AgentHarness/` owns current state and evidence.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
