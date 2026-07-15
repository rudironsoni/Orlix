---
type: epic
tags:
  - epic
  - orlix-release
updated: 2026-07-15
status: doing
summary: "Ship the signed Orlix application through the deterministic release harness and staged runtime proof order."
targets:
  - "[Orlix](../../product/orlix.md)"
has_story:
  - "[Validate and publish an Orlix release](../../story/doing/validate-and-publish-orlix-release.md)"
---

# Orlix release

Ship the signed Orlix application through the deterministic release harness and staged runtime proof order.

Promotion follows L0 source and policy checks, L1 unit and reducer proof, L2 component integration, L3 complete simulator product validation, optional authorized L4 identical physical-device validation, and L5 archive, export, upload, and App Store Connect confirmation. A release claim requires one exact product identity and semantic fingerprint across every required tier.

L3 and L4 exercise the same app-hosted capability contracts: application launch, OrlixOS payload and session, kernel boot, TCTI first syscall and stability, Linux console, BusyBox, OrlixMLibC, Coreutils, packages, loader, signals, VFS, OCI command execution, interactive terminal behavior, userspace marker provenance, and clean exit. A marker passes only when real Linux userspace emits it through the product session path. Harness text, argv diagnostics, and metadata cannot substitute.

Release inputs are checked in under `docs/sources/release/`. Archive and exported-app gates verify framework and payload shape, signing, entitlements, privacy declarations, provenance, license inputs, and target-derived metadata before an operator-authorized upload. Physical-device access and TestFlight publication remain explicit external actions.

Telemetry contracts fail closed when external infrastructure is unavailable. The product repository owns app-side event schemas, privacy filtering, configuration, and feature flags; backend provisioning and operations belong to the external infrastructure GitOps repository. Current readiness, blockers, report freshness, and the selected frontier belong to structured reports under `Build/AgentHarness/`.
