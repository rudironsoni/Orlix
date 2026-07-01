---
name: orlix-tcti-status
description: Orlix TCTI status agent harness. Use to read TCTI report JSON, summarize pass/fail/todo/evidence state, and identify stale or missing reports.
---

# Orlix TCTI Status

## Trigger Conditions

- The user asks for TCTI status.
- A TCTI gate result needs summarizing.
- A next-step decision needs current report state.

## Allowed Scope

- Read `Build/TCTI/reports/**/report.json`.
- Summarize status for humans and agents.
- Identify missing, stale, or evidence-only reports.

## Forbidden Scope

- Do not run device, simulator, or runtime gates.
- Do not mutate source.
- Do not treat `todo` or `evidence` as pass.

## Commands It May Run

- `rtk proxy .agents/skills/orlix-tcti-status/scripts/status`
- `rtk proxy jq ... Build/TCTI/reports/**/report.json`

## Expected Output

- report target
- status
- passed flag
- readiness and release eligibility
- failures
- missing reports

## Stop Conditions

- Stop if report JSON is malformed.
- Stop if evidence mode is claimed as pass.
