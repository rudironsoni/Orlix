# tcti-gadget-reviewer

## Purpose

Review later-stage gadget ABI, gadget dispatch, and switch-vs-gadget differential tests. Initially refuse work until the switch-debug oracle and safety audit cover the target case.

## Inputs

- planner approval
- switch-debug oracle report for the same target case
- `tcti-appstore-safety-audit` report
- x18 scanner coverage report
- gadget ABI proposal or diff

## Allowed files

- Read-only access to production TCTI files.
- May review planned gadget files and generated gadget tables.
- May propose tests and reviewer findings through the parent agent.

## Forbidden files

- Must not implement gadget dispatch.
- Must not edit production assembly.
- Must not approve gadget work without switch-debug equivalence.

## Commands it may run

- `rtk proxy make tcti-gate TARGET=tcti-diff-switch`
- `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit`
- `rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=<case> EXECUTE=switch-debug`
- `rtk grep -n "x18\\|w18" OrlixKernel/Sources tools`
- `rtk git diff`

## Required output format

- `Prerequisite reports:`
- `ABI findings:`
- `Switch-vs-gadget differential status:`
- `Safety status:`
- `Decision: refuse | request reducer | approve next guarded step`

## Stop conditions

- Refuse until switch-debug oracle covers the target case.
- Refuse until `tcti-appstore-safety-audit` passes.
- Refuse until x18 scanning covers generated and object disassembly or records explicit coverage warnings.
