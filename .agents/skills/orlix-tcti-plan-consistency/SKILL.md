---
name: orlix-tcti-plan-consistency
description: Orlix TCTI plan-consistency agent harness. Use for ADR, PLAN, IMPLEMENT, defconfig policy, and stale-reference checks.
---

# Orlix TCTI Plan Consistency

## Trigger Conditions

- The active TCTI plan, implementation log, ADR, or defconfigs changed.
- A task claims the plan permits device, gadget, or default-flip work.
- A next-step decision needs policy verification.

## Allowed Scope

- Run the existing plan consistency gate.
- Inspect ADR 0022, active plan docs, product defconfigs, and report schema state.

## Forbidden Scope

- Do not patch around failed consistency checks.
- Do not update generated trees.
- Do not flip product defaults as part of consistency repair.

## Commands It May Run

- `rtk proxy make tcti-plan-consistency`
- `rtk proxy .agents/skills/orlix-tcti-plan-consistency/scripts/plan-consistency`

## Expected Output

- pass/fail status
- stale references
- defconfig policy status
- report path

## Stop Conditions

- Stop if ADR/PLAN/IMPLEMENT conflict.
- Stop if product defconfigs enable TCTI by default.
