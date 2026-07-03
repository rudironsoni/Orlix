---
name: orlix-tcti-safety
description: Orlix TCTI safety agent harness. Use for App Store, x18, JIT, MAP_JIT, RWX, PROT_EXEC, HostAdapter, defconfig, generated-tree, or physical-device preflight checks.
---

# Orlix TCTI Safety

## Trigger Conditions

- A task mentions App Store, x18, JIT, MAP_JIT, RWX, PROT_EXEC, HostAdapter, defconfig, generated tree, or device preflight.
- TCTI runtime, gadget, or physical-device work is proposed.
- A TCTI change is being reviewed for readiness.

## Allowed Scope

- Run TCTI safety checks and plan consistency.
- Inspect product defconfigs, source diffs, generated-tree references, and TCTI reports.
- Block unsafe commands through skill-local hook scripts.

## Forbidden Scope

- Do not implement feature code.
- Do not flip product defconfigs to TCTI.
- Do not host-execute guest ELF text.
- Do not approve evidence mode as pass.

## Commands It May Run

- `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit`
- `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`
- `rtk proxy make agent-mcp-check`
- `rtk grep -n "x18\\|w18\\|MAP_JIT\\|RWX\\|PROT_EXEC\\|vm_protect"`

## Expected Output

- findings first
- forbidden behavior state
- reports reviewed
- defconfig state
- generated-tree state
- decision: `pass`, `fail`, or `evidence-only`

## Stop Conditions

- Stop on JIT, MAP_JIT, RWX, generated executable memory, host executable guest text, or forbidden x18/w18.
- Stop on HostAdapter-owned Linux semantics.
- Stop if product defconfigs default to TCTI before gates pass.
- Stop if physical device work bypasses runtime-validation preflight.
