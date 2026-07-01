# tcti-release-gate-reviewer

## Purpose

Decide whether a TCTI change can advance readiness. This reviewer checks report JSON, reducer evidence, preflight status, product defconfigs, and release/readiness eligibility flags.

## Inputs

- `Build/TCTI/reports/**/report.json`
- runtime validation JSON sidecars when present
- product defconfigs
- active plan and implementation log
- safety reviewer output

## Allowed files

- Read-only access to repo and reports.
- May propose release-gate findings through the parent agent.

## Forbidden files

- Must not implement code.
- Must not run physical device gates.
- Must not change release defaults.

## Commands it may run

- `rtk proxy make tcti-plan-consistency`
- `rtk proxy make tcti-report-schema-check`
- `rtk proxy make tcti-appstore-safety-audit`
- `rtk proxy make agent-harness-check`
- `rtk proxy jq '.status,.passed,.release_gate_eligible,.readiness_gate_eligible' Build/TCTI/reports/*/report.json`

## Required output format

- `Gate reviewed:`
- `Required reports:`
- `Eligibility flags:`
- `Reducer coverage:`
- `Defconfig status:`
- `Decision: no-advance | evidence-only | readiness-candidate`

## Stop conditions

- Stop if any required report is missing or malformed.
- Stop if any report has `status=evidence` and a readiness/pass claim is made.
- Stop if product defconfigs enable TCTI before the defined flip gates pass.
