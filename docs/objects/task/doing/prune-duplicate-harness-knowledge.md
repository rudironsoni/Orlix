---
type: task
tags: [task, agent-harness]
updated: 2026-09-13
status: doing
summary: "Reduce root context, focus skills, and report client capability limits."
task_of:
  - "[Reduce agent knowledge duplication and cost](../../story/doing/reduce-agent-knowledge-duplication-and-cost.md)"
depends_on:
  - "[Implement task-scoped execution](implement-task-scoped-execution.md)"
targets:
  - "[Agent harness](../../software-component/agent-harness.md)"
applies:
  - "[Agentic harness authority](../../../concepts/agentic-harness-authority.md)"
relates_to:
  - "[ADR 0033](../../architecture-decision/0033-use-bazel-as-the-repository-product-graph.md)"
owned_paths:
  - ".rulesync/**"
  - ".xcodebuildmcp/config.yaml"
  - "Tools/AgentHarness/**"
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
  - "rulesync"
  - "orlix-write-to-harness"
required_role: "orlix-reviewer"
required_proof:
  - "capability report derived from RuleSync output"
  - "compact root rule and progressive skill references"
  - "bounded XcodeBuildMCP default workflows"
build_intents:
  - "generate concise native roles and skills"
verification_intents:
  - "make agent-capabilities"
  - "make agent-harness-check"
---

# Prune duplicate harness knowledge

Keep specialized Bazel, Apple, TCTI, release, and binary procedures out of root context. Remove the generic project-context skill. Report instruction-only and unsupported client behavior without fallback enforcement.
