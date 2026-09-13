---
type: concept
tags:
  - agent-harness
  - ownership
updated: 2026-09-13
summary: "Separate durable work, portable agent configuration, generated clients, and current proof."
---

# Agentic harness authority

The ontology owns durable outcomes, work hierarchy, dependencies, acceptance boundaries, architecture references, and proof requirements. GitHub issues are its operational projection for discussion, notifications, and native execution state.

`.rulesync/` owns portable rules, skills, roles, static permissions, static MCP registration, and hook registration. RuleSync 16.26.1 owns client serialization. Generated client files contain no canonical portable knowledge.

`Tools/AgentHarness/` owns Orlix task context, semantic scope, task envelopes, lifecycle behavior, continuation state, proof checks, output inventory, and capability reporting. GitHub CI rejects generated changes in pull requests and performs authoritative regeneration on `main`. `Build/AgentHarness/` owns current task state, reports, proof, and raw evidence references.
