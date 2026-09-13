---
type: task
tags: [task, agent-harness]
updated: 2026-09-13
status: doing
summary: "Resolve one ontology task into semantic scope, lifecycle state, and proof checks."
task_of:
  - "[Scope agent execution to one task](../../story/doing/scope-agent-execution-to-one-task.md)"
depends_on:
  - "[Implement pinned RuleSync authority](implement-pinned-rulesync-authority.md)"
blocks:
  - "[Prune duplicate harness knowledge](prune-duplicate-harness-knowledge.md)"
targets:
  - "[Agent harness](../../software-component/agent-harness.md)"
applies:
  - "[Agentic harness authority](../../../concepts/agentic-harness-authority.md)"
relates_to:
  - "[ADR 0017](../../architecture-decision/0017-product-runtime-claim-promotion-order.md)"
owned_paths:
  - "Tools/AgentHarness/**"
  - ".rulesync/hooks.jsonc"
  - "Makefile"
  - "docs/**"
read_only_paths:
  - ".agents/**"
  - ".claude/**"
  - ".codex/**"
  - ".cursor/**"
forbidden_paths:
  - "Orlix/**"
  - "OrlixKernel/**"
required_skills:
  - "orlix-write-to-harness"
required_role: "orlix-implementer"
required_proof:
  - "selected task, story, epic, and typed context closure"
  - "unrelated doing-page exclusion"
  - "semantic path and generated destination rejection"
  - "structured continuation and proof-aware Stop behavior"
build_intents:
  - "compile current task state under Build/AgentHarness/agent"
verification_intents:
  - "make agent-task-envelope TASK=implement-task-scoped-execution"
  - "make agent-task-envelope-check"
  - "make agent-harness-check"
---

# Implement task-scoped execution

Compile one selected task and its typed context closure. Keep native permissions, MCP registration, model settings, approval policy, and sandbox settings outside the semantic task envelope. Lifecycle hooks use the envelope and current structured proof.
