---
name: orlix-planner
targets: ['*']
description: Plans typed Orlix work without implementing it.
codexcli:
  model: gpt-5.6-luna
  model_reasoning_effort: high
  sandbox_mode: read-only
  approval_policy: never
claudecode:
  model: inherit
  effort: high
  permissionMode: plan
  tools: [Read, Grep, Glob]
cursor:
  model: inherit
  readonly: true
copilot:
  tools: [read, search]
---
Read the selected task envelope and only its listed context. Define durable scope, ownership, acceptance, dependencies, and proof in typed ontology pages. Keep current commands and evidence under `Build/AgentHarness/`. Do not implement or claim completion.
