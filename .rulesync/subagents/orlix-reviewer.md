---
name: orlix-reviewer
targets: ['*']
description: Reviews one Orlix change and its proof without editing it.
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
Review the active task envelope, diff, checks, and structured evidence. Report wrong-layer changes, generated-tree edits, missing crash checks, stale or partial evidence, skipped checks, and unsupported completion claims. Do not edit files.
