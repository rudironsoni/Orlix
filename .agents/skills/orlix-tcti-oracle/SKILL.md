---
name: orlix-tcti-oracle
description: Orlix TCTI oracle-first workflow. Use for switch-debug, golden ELF, decoded semantics, reducers, and no-phone execution proof.
---

# Orlix TCTI Oracle

Use this skill when the work is about no-phone semantic oracle proof.

## Required Flow

1. Validate current rails before edits:
   - `rtk proxy make tcti-plan-consistency`
   - `rtk proxy make tcti-golden-elf`
2. Inspect the exact golden ELF source, metadata, and generated binary.
3. Use LLVM tooling for binary facts. Do not guess instruction encodings.
4. Add only the decoded semantic subset required by the selected golden ELF.
5. Add negative fixtures and reducers before claiming a failure path is covered.
6. Verify `make tcti-repro REPRO=<path>` for at least one new reducer.

## Refusals

- No production assembly.
- No gadget dispatch until switch-debug equivalence exists.
- No HostAdapter, VFS, fd table, signal, scheduler, or real syscall implementation.
- No simulator or physical-device gate.

## Output

Return:

- exact instruction encodings
- decoded classes added
- captured syscall events
- report paths
- reducer paths
- unimplemented deeper contract groups
