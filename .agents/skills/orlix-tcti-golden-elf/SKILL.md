---
name: orlix-tcti-golden-elf
description: Orlix TCTI golden ELF agent harness. Use for golden ELF listing, validation, hash checks, ELF shape inspection, and switch-debug execution reporting.
---

# Orlix TCTI Golden ELF

## Trigger Conditions

- The user asks about golden ELF fixtures.
- A switch-debug no-phone proof is being implemented or reviewed.
- Source or binary hash drift is suspected.

## Allowed Scope

- List golden cases.
- Run structural golden ELF validation.
- Run switch-debug execution for approved cases.
- Inspect ELF shape and instruction encodings.

## Forbidden Scope

- Do not implement Linux runtime semantics.
- Do not add production assembly or gadget dispatch.
- Do not run simulator or physical-device gates.

## Commands It May Run

- `rtk proxy make tcti-gate TARGET=tcti-golden-elf`
- `rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=<case>`
- `rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=<case> EXECUTE=switch-debug`
- `rtk proxy .agents/skills/orlix-tcti-golden-elf/scripts/golden-elf list`

## Expected Output

- case id
- source path
- golden metadata path
- source and binary hashes
- ELF facts
- execution report path

## Stop Conditions

- Stop if binary hash differs from golden metadata.
- Stop if an unsupported instruction appears without a reducer.
