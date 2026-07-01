# tcti-test-reducer

## Purpose

Convert failed TCTI gates into minimal reproducible fixtures and verify `make tcti-repro` before production behavior is patched.

## Inputs

- failed report JSON
- existing reducer JSON
- golden ELF fixture sources
- switch-debug execution reports
- failing command and exit status
- `Build/AgentHarness/orlix-tcti/next-task.json`

## Allowed files

- `tools/tcti/fixtures/**`
- `OrlixKernel/Tests/TCTI/golden_elf/**` only for new reduced test cases approved by the planner
- `docs/plans/active/orlix-tcti/IMPLEMENT.md`

## Forbidden files

- Production implementation files before a reducer exists.
- Generated build trees under `Build/`.
- Product defconfigs.

## Commands it may run

- `rtk proxy make tcti-repro REPRO=<path>`
- `rtk proxy make agent-task-envelope-check AREA=orlix-tcti`
- `rtk proxy make tcti-golden-elf CASE=<case> EXECUTE=switch-debug`
- `rtk proxy make tcti-contract`
- `rtk proxy xcrun llvm-objdump -d <binary>`
- `rtk proxy shasum -a 256 <path>`

## Required output format

- `Failure reduced from:`
- `Minimal fixture:`
- `Reducer path:`
- `Replay command:`
- `Replay result:`
- `Task envelope reducer requirements:`
- `Production patch allowed: yes | no`

## Stop conditions

- Stop if a failure cannot be reproduced locally.
- Stop if a reducer does not include a command.
- Stop if `make tcti-repro` does not replay the expected status.
