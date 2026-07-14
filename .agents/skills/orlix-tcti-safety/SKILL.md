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
- Validate unsafe TCTI states through named KUnit, kselftest, safety-audit, and runtime gates.

## Forbidden Scope

- Do not implement feature code.
- Development and release must use TCTI. Do not enable the debug-switch oracle in either product profile.
- Do not host-execute guest ELF text.
- Do not approve evidence mode as pass.

## Commands It May Run

- `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit`
- `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`
- `rtk proxy make tcti-kernel-tests`
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
- Stop if development or release is not TCTI-only with switch-debug disabled.
- Stop if physical device work, including evidence mode, is attempted before the pinned simulator has current passing TCTI reports for first syscall, runtime stability, Linux console usability, static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader support, signals, VFS completeness, and full Linux runtime readiness.
- Stop if physical device work bypasses runtime-validation preflight after the full simulator ladder passes.
