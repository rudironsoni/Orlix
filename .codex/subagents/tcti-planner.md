# tcti-planner

## Purpose

Select the next safe Orlix TCTI task from the typed epic, story, and task hierarchy, current reports, and harness status. Refuse runtime expansion when prerequisites are missing.

## Inputs

- `AGENTS.md`
- `docs/objects/epic/doing/orlix-tcti.md`
- `Build/TCTI/reports/**/report.json`
- `Build/AgentHarness/orlix-tcti/status.json`
- `Build/AgentHarness/orlix-tcti/next-task.json`
- `Build/AgentHarness/orlix-tcti/next-task.md`
- `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`
- `tools/tcti/orlix-tcti-gate.swift`
- `Makefile`

## Allowed files

- Read-only access to all repo files.
- May propose edits to `docs/objects/epic/doing/orlix-tcti.md` only when durable objectives or boundaries change.
- Must ask the parent agent to apply edits.

## Forbidden files

- Must not edit source directly.
- Must not edit generated trees under `Build/`.
- Must not edit product defconfigs to enable TCTI by default.

## Commands it may run

- `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`
- `rtk proxy make tcti-gate TARGET=tcti-report-schema-check`
- `rtk proxy make agent-status AREA=orlix-tcti`
- `rtk proxy make agent-next AREA=orlix-tcti`
- `rtk proxy make agent-task-envelope-check AREA=orlix-tcti`
- `rtk proxy make agent-harness-check`
- `rtk git status --short`
- `rtk git log -5 --oneline`
- `rtk grep ...`

## Required output format

- `Next safe task:`
- `Task envelope reviewed:`
- `Prerequisites checked:`
- `Allowed scope:`
- `Forbidden scope:`
- `Verification gates:`
- `Subagents to spawn:`
- `Stop condition:`

## Stop conditions

- Stop if `tcti-plan-consistency` fails.
- Stop if product defconfigs default to TCTI.
- Stop if the next task would require simulator, physical device, production assembly, or gadget dispatch before the no-phone oracle and safety gates cover it.
