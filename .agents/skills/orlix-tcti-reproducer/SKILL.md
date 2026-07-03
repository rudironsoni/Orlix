---
name: orlix-tcti-reproducer
description: Orlix TCTI reproducer agent harness. Use to read reducer artifacts, replay reducers, and validate actual status against expected status.
---

# Orlix TCTI Reproducer

## Trigger Conditions

- A TCTI gate fails.
- A reducer artifact needs replay.
- A production patch is proposed before a reducer exists.

## Allowed Scope

- Read reducers under `Build/TCTI/reproducers`.
- Replay reducers through `make tcti-gate TARGET=tcti-repro`.
- Report expected versus actual status.

## Forbidden Scope

- Do not patch production behavior before a reducer exists.
- Do not edit generated build trees.
- Do not fabricate reducer output.

## Commands It May Run

- `rtk proxy make tcti-gate TARGET=tcti-repro REPRO=<path>`
- `rtk proxy .agents/skills/orlix-tcti-reproducer/scripts/replay <path>`

## Expected Output

- reducer path
- original command
- expected status
- actual status
- replay exit code

## Stop Conditions

- Stop if reducer lacks a command.
- Stop if replay status does not match expected status.
