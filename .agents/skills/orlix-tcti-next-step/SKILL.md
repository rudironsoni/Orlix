---
name: orlix-tcti-next-step
description: Orlix TCTI next-step harness. Use when the user says continue TCTI work, run the harness, what next, pick the next safe task, or coordinate TCTI subagents.
---

# Orlix TCTI Next Step

Use this skill before implementing new TCTI work.

## Required Flow

1. Read `AGENTS.md`, `docs/plans/active/orlix-tcti/PLAN.md`, and `docs/plans/active/orlix-tcti/IMPLEMENT.md`.
2. Run or inspect:
   - `rtk proxy make tcti-plan-consistency`
   - `rtk proxy make codex-harness-check`
   - latest `Build/TCTI/reports/**/report.json`
3. Spawn or simulate these roles when subagent tooling is available:
   - `tcti-planner`
   - `tcti-safety-reviewer`
   - one relevant implementation role such as `tcti-oracle-engineer`, `tcti-llvm-inspector`, or `tcti-test-reducer`
4. Choose one next gate and one implementation surface.
5. Do not code until the gate, scope, and forbidden work are explicit.

## Refusals

- Do not implement production TCTI assembly.
- Do not implement gadget dispatch.
- Do not run simulator or physical-device gates unless the preflight permits it.
- Do not expand Linux runtime semantics.
- Do not bypass planner and safety reviewer for TCTI changes.

## Output

Return:

- `Selected gate`
- `Subagents used`
- `Allowed files`
- `Forbidden files`
- `Commands to run`
- `Stop condition`
