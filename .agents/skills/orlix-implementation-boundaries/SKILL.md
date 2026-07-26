---
name: orlix-implementation-boundaries
description: >-
  Use before implementing or triaging Orlix changes when ownership could involve
  OrlixOS, OrlixKernel, OrlixMLibC, OrlixCoreUtils, OrlixMachine, Containers,
  Herdr, OrlixTCTI, OrlixHostAdapter, the native Orlix app, upstream Linux,
  upstream mlibc, packages, product payloads, session APIs, or generated trees.
  Routes fixes to the correct layer and blocks wrong-layer shortcuts.
---
# Orlix Implementation Boundaries

Use this skill before changing code for non-trivial Orlix behavior, especially kernel/libc/package/runtime issues.

## Routing

- Linux semantics: `OrlixKernel/Sources/ports/orlix`, upstream Linux overlay paths, Linux-native drivers, boot, KUnit, or kselftest.
- Libc behavior: `OrlixMLibC/Sources` and `OrlixMLibC/Tests`.
- Core userspace utilities: private `OrlixCoreUtils`, using normal Linux process and libc behavior.
- Public delivered OS Kit, app-facing API, target-derived payload metadata, curated distribution policy, and package/rootfs assembly: `OrlixOS`.
- Linux machine and session composition behind the public Kit: `OrlixMachine`.
- Container lifecycle and machine integration: `Containers`.
- Terminal Session, Workspace, Tab, Pane, focus, and topology: Herdr.
- Guest instruction translation and execution beneath Linux: `OrlixTCTI`.
- Private Apple and Darwin execution mechanics: `OrlixHostAdapter/Sources`.
- iOS and iPadOS application UI and presentation of Herdr-owned terminal state: the native Orlix application source compiled directly into the production app target.
- Architecture truth: typed decision, component, capability, and concept pages reachable from `docs/index.md`.

## Refusals

Do not:

- edit generated upstream trees or disposable build output; reading generated trees for diagnosis is allowed;
- add or change durable upstream patches before proving the lower owning layer is compliant and the patch is not hiding an OrlixKernel, OrlixOS, or toolchain mismatch;
- modify upstream tests through durable patch stacks used by conformance schemes; move extra regressions to Orlix-owned tests;
- move Linux policy into `OrlixHostAdapter`;
- move guest instruction decoding into `OrlixHostAdapter`;
- move libc behavior into `OrlixKernel`;
- move syscall semantics into `OrlixOS`;
- recreate a separate `OrlixKit` target/module; `OrlixOS` is the Kit;
- expose private `OrlixKernel`, `OrlixMLibC`, or `OrlixCoreUtils` as parallel app-facing APIs;
- duplicate Herdr's Session, Workspace, Tab, Pane, focus, or topology state in the native app or another Orlix layer;
- move terminal topology into `OrlixOS`, `OrlixMachine`, or `Containers`;
- create a reusable `OrlixTerminal` framework or generic product boundary around the native Orlix application;
- restore the retired UIKit terminal controller, its separate diagnostic app target, or its conflicting Ghostty package;
- hardcode product bundle identifiers or payload resource names in runtime code when they belong in `project.yml` or target metadata;
- disable upstream package capabilities or invent package-specific linker/tool wrappers instead of fixing OrlixOS package-toolchain inputs;
- add public runtime facades or shell/package management APIs;
- restore retired local-kernel prototype paths.

## Output

Before implementation, state the owning layer, why that layer owns the behavior, the files to inspect first, and the verification gate.
