---
name: orlix-write-to-harness
description: Use before editing the Orlix ontology brain, AGENTS.md, agent skills, subagents, hooks, generated rules, or harness guidance. Preserves repository source of truth and prevents duplicated or stale status.
---

# Orlix Write To Harness

Use this skill before writing any repository knowledge or agent-harness surface.

## Rules

- Keep `AGENTS.md` a concise router into `docs/index.md`, typed knowledge pages, and structured reports.
- Follow `docs/ontology.md` and `docs/AGENTS.md` for every knowledge mutation.
- Keep durable architecture, ownership, capabilities, and objectives in typed pages under `docs/objects/` and `docs/concepts/`.
- Keep current task selection, commands, evidence, failures, and readiness state in structured reports under `Build/AgentHarness/`.
- Do not create plan journals, implementation logs, handoff archives, or copied status snapshots.
- Update `docs/log.md`, regenerate `docs/index.md`, and repair every consumer when knowledge changes.
- Keep OrlixOS as the Kit, upstream Linux as owner of Linux behavior, and OrlixHostAdapter limited to private Apple mechanics.
- Edit `.rulesync/` as the durable source for generated rules and subagents. Generate only the intended features so hand-maintained skills remain intact.
- Treat bare commands and `rtk`-wrapped equivalents identically in hooks and rules.

## Verification

Run `make docs-check`, `make agent-harness-check`, a stale legacy-path scan, and the focused tests for any changed hook or workflow.
