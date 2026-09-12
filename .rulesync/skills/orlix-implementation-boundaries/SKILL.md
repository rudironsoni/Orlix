---
name: orlix-implementation-boundaries
description: >-
  Use before implementing or triaging Orlix changes when ownership could involve
  OrlixKit, OrlixEngine, OrlixOS, OrlixInstance, OrlixContainer,
  Herdr, OrlixTCTI, OrlixHostAdapter, the native Orlix app, upstream Linux,
  upstream mlibc, packages, product payloads, session APIs, or generated trees.
  Routes fixes to the correct layer and blocks wrong-layer shortcuts.
targets:
  - '*'
---
# Orlix Implementation Boundaries

Use this skill before changing code for non-trivial Orlix behavior, especially kernel/libc/package/runtime issues.

## Routing

- Linux semantics: `OrlixKernel/Sources/ports/orlix`, upstream Linux overlay paths, Linux-native drivers, boot, KUnit, or kselftest.
- Libc behavior: `OrlixMLibC/Sources` and `OrlixMLibC/Tests`.
- Core userspace utilities: private `OrlixCoreUtils`, using normal Linux process and libc behavior.
- Public Swift API and complete embeddable Apple product: `OrlixKit`.
- Process-wide host lifecycle and boot orchestration: `OrlixEngine`.
- Private native boot mechanism: `OrlixBootloader`.
- Running hosted OS with one upstream Linux kernel: `OrlixOS`.
- Persistent isolated userspace, ordinary processes, and OCI containers: `OrlixInstance`, `OrlixProcess`, and `OrlixContainer`, managed through Linux userspace supervision.
- Curated guest distribution and package/rootfs resources: private distribution assembly consumed by OrlixKit packaging. OrlixMLibC, Coreutils, and guest packages are not Apple-native link dependencies.
- Terminal Session, Workspace, Tab, Pane, focus, and topology: Herdr.
- Guest instruction translation and execution beneath Linux: `OrlixTCTI`.
- Private Apple and Darwin execution mechanics: `OrlixHostAdapter/Sources`.
- iOS and iPadOS UI, vvterm/Ghostty terminal, remote connections, and presentation of Herdr-owned state: private native app modules composed into Orlix.app.
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
- bypass `OrlixKit` with direct app dependencies on private local-runtime implementation;
- hide guest libraries or package-provider link assertions inside a native aggregate target;
- expose private `OrlixKernel`, `OrlixMLibC`, or `OrlixCoreUtils` as parallel app-facing APIs;
- duplicate Herdr's Session, Workspace, Tab, Pane, focus, or topology state in the native app or another Orlix layer;
- move terminal topology into `OrlixKit`, `OrlixEngine`, `OrlixOS`, `OrlixInstance`, or `OrlixContainer`;
- create a reusable `OrlixTerminal` framework or generic product boundary around the native Orlix application;
- restore the retired UIKit terminal controller, its separate diagnostic app target, or its conflicting Ghostty package;
- hardcode product bundle identifiers or payload resource names in runtime code when they belong in `project.yml` or target metadata;
- disable upstream package capabilities or invent package-specific linker/tool wrappers instead of fixing OrlixOS package-toolchain inputs;
- implement Linux process, syscall, namespace, or container semantics in a Darwin-side facade;
- restore retired local-kernel prototype paths.

## Output

Before implementation, state the owning layer, why that layer owns the behavior, the files to inspect first, and the verification gate.

ADR 0040 governs the recovery vocabulary. Earlier SDK/lifecycle naming clauses do not prohibit this migration. Acceptance of the architecture is not proof that the corresponding source or runtime is complete.
