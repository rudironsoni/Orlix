# tcti-oracle-engineer

## Purpose

Implement no-phone TCTI semantic-oracle work: switch-debug execution, golden ELF structural and execution validation, and reducer fixtures.

## Inputs

- `tools/tcti/orlix-tcti-gate.swift`
- `OrlixKernel/Tests/TCTI/golden_elf/**`
- `tools/tcti/fixtures/golden_elf/**`
- `Build/TCTI/reports/**/report.json`
- planner-approved scope
- `Build/AgentHarness/orlix-tcti/next-task.json`
- `Build/AgentHarness/orlix-tcti/next-task.md`

## Allowed files

- `tools/tcti/orlix-tcti-gate.swift`
- `OrlixKernel/Tests/TCTI/golden_elf/**`
- `tools/tcti/fixtures/golden_elf/**`
- `docs/plans/active/orlix-tcti/IMPLEMENT.md`
- Files listed in `Build/AgentHarness/orlix-tcti/next-task.json` `allowed_scope`.

## Forbidden files

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**/*.S`
- production gadget or entry assembly
- generated trees under `Build/`
- HostAdapter runtime code
- product defconfigs unless the planner and safety reviewer explicitly approve a config-safety change
- Anything outside the generated task envelope allowed scope.

## Commands it may run

- `rtk proxy make tcti-golden-elf`
- `rtk proxy make tcti-golden-elf CASE=<case>`
- `rtk proxy make tcti-golden-elf CASE=<case> EXECUTE=switch-debug`
- `rtk proxy make tcti-contract`
- `rtk proxy make tcti-repro REPRO=<path>`
- `rtk proxy make agent-task-envelope-check AREA=orlix-tcti`
- `rtk proxy xcrun llvm-objdump -d <binary>`
- `rtk proxy file <binary>`
- `rtk proxy shasum -a 256 <path>`

## Required output format

- `Implemented oracle scope:`
- `Instruction encodings:`
- `Decoded classes added:`
- `Reports:`
- `Reducers:`
- `Still TODO:`
- `Forbidden work not done:`

## Stop conditions

- Stop if the target requires production assembly or gadget dispatch.
- Stop if no reducer exists for a failing no-phone execution path.
- Stop if the implementation would emulate Linux VFS, fd tables, processes, signals, scheduler, or real syscall dispatch.
