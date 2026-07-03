---
name: orlix-tcti-oracle
description: Orlix TCTI oracle agent harness. Use for switch-debug, golden ELF, decoded semantics, reducers, and no-phone execution proof.
---

# Orlix TCTI Oracle

## Trigger Conditions

- The task mentions switch-debug, golden ELF, decoded semantics, reducers, or no-phone execution.
- A semantic oracle change is proposed before gadget work.

## Allowed Scope

- Implement no-phone switch-debug oracle work.
- Add golden ELF structural and execution validation.
- Add reducer fixtures before fixing behavior.

## Forbidden Scope

- Do not implement production assembly.
- Do not implement gadget dispatch.
- Do not implement HostAdapter, VFS, fd table, signal, scheduler, process, or real syscall behavior.
- Do not run simulator or physical-device gates.

## Commands It May Run

- `rtk proxy make tcti-gate TARGET=tcti-golden-elf`
- `rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=<case> EXECUTE=switch-debug`
- `rtk proxy make tcti-gate TARGET=tcti-repro REPRO=<path>`
- `rtk proxy xcrun llvm-objdump -d <binary>`

## Expected Output

- instruction encodings
- decoded classes added
- captured syscall events
- report paths
- reducer paths
- still-TODO groups

## Stop Conditions

- Stop if the target requires production assembly or gadget dispatch.
- Stop if no reducer exists for a failing path.
- Stop if Linux runtime semantics would be implemented in the oracle.
