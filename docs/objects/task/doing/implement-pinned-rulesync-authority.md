---
type: task
tags: [task, agent-harness]
updated: 2026-09-13
status: doing
summary: "Pin RuleSync, validate source, protect generated output, and regenerate on main."
task_of:
  - "[Establish agent configuration authority](../../story/doing/establish-agent-configuration-authority.md)"
blocks:
  - "[Implement task-scoped execution](implement-task-scoped-execution.md)"
targets:
  - "[Agent harness](../../software-component/agent-harness.md)"
applies:
  - "[Agentic harness authority](../../../concepts/agentic-harness-authority.md)"
owned_paths:
  - ".rulesync/**"
  - "rulesync.jsonc"
  - "Tools/AgentHarness/**"
  - ".github/workflows/rulesync-*.yml"
  - "Makefile"
  - "docs/**"
read_only_paths:
  - ".agents/**"
  - ".claude/**"
  - ".codex/**"
  - ".cursor/**"
  - ".github/agents/**"
  - ".github/hooks/**"
  - ".github/skills/**"
forbidden_paths:
  - "Orlix/**"
  - "OrlixKernel/**"
required_skills:
  - "rulesync"
  - "orlix-write-to-harness"
required_role: "orlix-implementer"
required_proof:
  - "RuleSync 16.26.1 source validation"
  - "base and head generated ownership guard"
  - "validated main generator write set"
build_intents:
  - "generate tool-native agent configuration on main"
verification_intents:
  - "make agent-rules-check"
  - "make agent-rules-inventory"
  - "make agent-capabilities"
---

# Implement pinned RuleSync authority

Keep portable configuration in `.rulesync/`. Derive generated ownership with RuleSync for both pull-request revisions. Reject generated pull-request changes. Regenerate and validate the full write set on `main` before committing it.
