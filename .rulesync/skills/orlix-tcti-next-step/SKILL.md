---
name: orlix-tcti-next-step
description: >-
  Select the next owning OrlixTCTI test suite and emit a narrow scope envelope
  without reimplementing or interpreting test behavior.
targets:
  - '*'
---
# OrlixTCTI Next Step

Use this skill only to route work to the owning test layer.

## Required reads

- `docs/objects/task/doing/complete-aarch64-isa-on-isa-coverage.md`
- `docs/objects/task/doing/complete-pinned-simulator-tcti-ladder.md`
- `docs/objects/story/doing/prove-tcti-product-execution.md`
- `.agents/skills/orlix-implementation-boundaries/SKILL.md`
- `.agents/skills/orlix-runtime-claim-verification/SKILL.md`

## Commands

Run, in order:

```sh
make agent-status AREA=orlix-tcti
make agent-next AREA=orlix-tcti
make agent-task-envelope-check AREA=orlix-tcti
```

The resulting `Build/AgentHarness/orlix-tcti/next-task.json` is a scope contract. It selects complete-target ISA coverage until that task is done, lists its C-native audit before the owning test suites in ADR 0017 order, but it does not execute tests, parse their output, infer readiness, or replace their native result formats.

## Test ownership

- Complete target inventory, feature-domain satisfiability, classification, and proof-registry integrity: OrlixKernel C-native ISA audit.
- OrlixTCTI instruction execution and structured exits: kernel KUnit.
- Linux-visible process, syscall, signal, PTY, and terminal behavior: Linux kselftest.
- libc behavior: upstream mlibc tests.
- package behavior: upstream Coreutils tests.
- private Darwin transport and memory behavior: OrlixHostAdapter XCTest.
- OS session and payload behavior: OrlixOS XCTest.
- application integration and presentation: native app XCTest.

Do not add a host-side OrlixTCTI behavioral model, golden-ELF oracle, reducer protocol, report interpreter, or aggregate pass/fail gate here.
