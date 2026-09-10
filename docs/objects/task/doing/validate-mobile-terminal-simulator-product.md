---
type: task
tags:
  - task
  - release
  - mobile-terminal
updated: 2026-09-10
status: doing
summary: "Validate the complete mobile terminal product on the selected simulator."
task_of:
  - "[Validate and publish the mobile terminal release](../../story/doing/validate-and-publish-mobile-terminal-release.md)"
depends_on:
  - "[Keep Herdr authoritative for terminal topology](keep-herdr-authoritative-for-terminal-topology.md)"
  - "[Validate mobile platform presentation](../todo/validate-mobile-platform-presentation.md)"
  - "[Prove concurrent OrlixMachine isolation](../todo/prove-concurrent-orlix-machine-isolation.md)"
  - "[Promote TCTI as the product default](../todo/promote-tcti-as-product-default.md)"
blocks:
  - "[Validate an authorized mobile terminal device](../todo/validate-authorized-mobile-terminal-device.md)"
  - "[Archive, export, and upload the mobile terminal release](../todo/archive-export-and-upload-mobile-terminal-release.md)"
---

# Validate the mobile terminal simulator product

Advance the exact mobile terminal release candidate through source policy, focused owning regression tests, component integration, and complete simulator product validation while preserving one semantic product fingerprint. This task proves the terminal, commercially available Herdr, OrlixMachine, and OrlixTCTI contract. OCI and Docker behavior belongs to the later mobile container release.

The iOS local Orlix terminal and remote panes must use the same `TerminalPaneSurface`, Ghostty appearance settings, surface registry, and keyboard behavior. Their session backends supply input, output, and resize handling. Init diagnostics use Linux `/dev/kmsg` before the console becomes a raw transport. The terminal UI must preserve PTY bytes.

`make __bazel-test-terminal-surface` checks local terminal opening, keyboard dismissal, reopening by tap and the shared Keyboard control, and typing, plus the existing remote SSH background, keyboard, and typing regression. It uses `ORLIX_TEST_DESTINATION` and disables parallel testing. These UI checks do not establish POSIX shell conformance or complete the mobile release gate.

The local software-keyboard test uses the system hardware-keyboard state without an override. A completed terminal focus tap and the shared Keyboard control both call the coordinator's explicit show action; scrolling and selection do not. The remote test requires the existing loopback SSH fixture on `127.0.0.1:22229`, with its username and key configured in the harness defaults. `ORLIX_APP_TEST_ONLY_TESTING` selects one test for diagnosis; a selected test does not replace the complete gate.
