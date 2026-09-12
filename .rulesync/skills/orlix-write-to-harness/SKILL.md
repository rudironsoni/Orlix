---
name: orlix-write-to-harness
description: >-
  Use before editing the Orlix ontology brain, AGENTS.md, agent skills,
  subagents, hooks, generated rules, or harness guidance. Preserves repository
  source of truth and prevents duplicated or stale status.
targets:
  - '*'
---
# Orlix Write To Harness

Use this skill before writing any repository knowledge or agent-harness surface.

## Rules

- Keep `AGENTS.md` a concise router into `docs/index.md`, typed knowledge pages, and structured reports.
- Follow `docs/ontology.md` and `docs/AGENTS.md` for every knowledge mutation.
- Keep durable architecture, ownership, capabilities, outcomes, stories, and tasks in typed pages under `docs/objects/` and `docs/concepts/`.
- Organize work as `epic -> story -> task`, and keep each work page in its matching `todo/`, `doing/`, or `done/` folder.
- Keep current task selection, commands, evidence, failures, and readiness state in structured reports under `Build/AgentHarness/`.
- Record each independently verified PR #228 recovery checkpoint in `IMPLEMENT.md`, as required by the accepted plan. Link to raw structured evidence instead of duplicating it. Do not create parallel journals or status archives.
- Update `docs/log.md`, regenerate `docs/index.md`, and repair every consumer when knowledge changes.
- Keep OrlixKit as the public SDK and OrlixOS as the running OS. Preserve upstream Linux ownership and the separation between private native implementation and guest distribution artifacts.
- Edit `.rulesync/` as the durable source for generated rules and subagents. Generate only the intended features so hand-maintained skills remain intact.
- Preserve and identify user-owned generated-file deltas before regeneration. Reapply or incorporate their intent without loss and record the preserved delta and generated diff in the checkpoint.
- Treat command wrappers identically to their underlying commands in hooks and rules.

## Verification

Run `make docs-check`, `make agent-harness-check`, a stale legacy-path scan, and the focused tests for any changed hook or workflow.
