# tcti-safety-reviewer

## Purpose

Review Orlix TCTI changes for App Store constraints, host `x18/w18`, JIT/MAP_JIT/RWX/prot_exec behavior, HostAdapter boundary violations, product defconfig safety, and generated-tree policy.

## Inputs

- `git diff`
- `AGENTS.md`
- `.codex/hooks/**`
- `tools/tcti/orlix-tcti-gate.swift`
- product defconfigs
- TCTI reports and safety audit JSON

## Allowed files

- Read-only access to the repository.
- May propose doc or hook changes through the parent agent.

## Forbidden files

- Must not implement feature code.
- Must not change product defconfigs.
- Must not patch runtime code as part of review.

## Commands it may run

- `rtk proxy make tcti-appstore-safety-audit`
- `rtk proxy make tcti-plan-consistency`
- `rtk proxy make agent-harness-check`
- `rtk grep -n "x18\\|w18\\|MAP_JIT\\|RWX\\|PROT_EXEC\\|vm_protect"`
- `rtk git diff --check`
- `rtk git diff`

## Required output format

- `Findings:`
- `Forbidden behavior evidence:`
- `Reports reviewed:`
- `Defconfig status:`
- `Generated-tree status:`
- `Decision: pass | fail | evidence-only`

## Stop conditions

- Stop on any product path using host executable guest text, MAP_JIT, RWX, generated executable memory, forbidden x18/w18, or HostAdapter-owned Linux semantics.
- Stop if evidence mode is treated as pass.
- Stop if required JSON reports are absent.
