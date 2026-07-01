---
name: orlix-tcti-next-step
description: Orlix TCTI next-step agent harness. Use when continuing TCTI work, running the harness, choosing what is next, or coordinating planner and safety review.
---

# Orlix TCTI Next Step

## Trigger Conditions

- The user asks to continue TCTI work.
- The user says "run the harness" or asks "what next".
- A TCTI implementation task is proposed without a clear gate.

## Allowed Scope

- Read `AGENTS.md`, `PLAN.md`, `IMPLEMENT.md`, and TCTI reports.
- Run agent-neutral harness checks and TCTI no-phone status checks.
- Spawn or simulate planner, safety reviewer, LLVM inspector, oracle engineer, or reducer roles.
- Produce the next safe task and its verification gates.

## Forbidden Scope

- Do not implement TCTI runtime features directly.
- Do not run simulator or physical-device gates.
- Do not add production assembly or gadget dispatch.
- Do not bypass planner and safety reviewer for TCTI work.

## Commands It May Run

- `rtk proxy make agent-harness-check`
- `rtk proxy make tcti-plan-consistency`
- `rtk proxy make tcti-report-schema-check`
- `rtk proxy make tcti-golden-elf`
- `rtk git status --short`

## Expected Output

- selected next gate
- subagents used
- allowed files
- forbidden files
- commands to run
- stop condition

## Stop Conditions

- Stop if `agent-harness-check` fails.
- Stop if `tcti-plan-consistency` fails.
- Stop if the next task needs device, simulator, production assembly, or gadget work before no-phone prerequisites pass.
