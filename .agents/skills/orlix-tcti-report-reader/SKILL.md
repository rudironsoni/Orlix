---
name: orlix-tcti-report-reader
description: Orlix TCTI report-reader agent harness. Use to read report JSON, validate schema facts, and expose report fields without arbitrary shell behavior.
---

# Orlix TCTI Report Reader

## Trigger Conditions

- The user asks to inspect a TCTI report.
- A subagent needs report facts.
- A readiness claim cites a TCTI report.

## Allowed Scope

- Read allowlisted reports under `Build/TCTI/reports`.
- Validate reports through the existing schema gate.
- Extract status, failures, counters, artifacts, and execution facts.

## Forbidden Scope

- Do not run arbitrary shell commands.
- Do not mutate reports.
- Do not claim report pass if schema validation fails.

## Commands It May Run

- `rtk proxy make tcti-gate TARGET=tcti-report-schema-check`
- `rtk proxy .agents/skills/orlix-tcti-report-reader/scripts/read-report <target>`
- `rtk proxy jq ... Build/TCTI/reports/<target>/report.json`

## Expected Output

- report path
- schema status
- selected report fields
- uncertainty if missing or malformed

## Stop Conditions

- Stop if target is not allowlisted.
- Stop if report JSON is missing or malformed.
