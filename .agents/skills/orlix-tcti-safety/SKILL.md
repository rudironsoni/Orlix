---
name: orlix-tcti-safety
description: Orlix TCTI safety review. Use for App Store, x18, JIT, MAP_JIT, RWX, PROT_EXEC, HostAdapter, defconfig, generated tree, or physical-device preflight questions.
---

# Orlix TCTI Safety

Use this skill before reviewing or changing any TCTI safety boundary.

## Required Flow

1. Run:
   - `rtk proxy make tcti-appstore-safety-audit`
   - `rtk proxy make tcti-plan-consistency`
2. Inspect product defconfigs for TCTI/default debug switch.
3. Search source and generated outputs for `x18`, `w18`, `MAP_JIT`, `RWX`, `PROT_EXEC`, and `vm_protect`.
4. Confirm HostAdapter does not own instruction decoding, syscall dispatch, VFS, fd tables, signal, process, or CPU feature policy.

## Refusals

- Reject product paths that host-execute guest ELF text.
- Reject generated executable memory, MAP_JIT, RWX, or private executable-memory entitlements.
- Reject physical-device TCTI work without autonomous preflight reports or explicit evidence mode.
- Reject changes under generated Linux trees.

## Output

Return findings first, then:

- reports reviewed
- defconfig state
- forbidden behavior state
- decision: `pass`, `fail`, or `evidence-only`
