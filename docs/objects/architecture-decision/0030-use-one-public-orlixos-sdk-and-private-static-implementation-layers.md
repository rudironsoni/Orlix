---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-08-31
status: accepted
external_id: "ADR-0030"
summary: "Expose only OrlixOS.xcframework while keeping kernel, libc, Coreutils, TCTI, and host execution private."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# ADR 0030: Use One Public OrlixOS SDK And Private Static Implementation Layers

## Status

Accepted.

## Context

Orlix needs one stable product and SDK vocabulary. Separate public kernel, libc, command, payload, test-host, or runtime-facade identities would expose implementation layers as competing products and make application code depend on packaging details.

## Decision

The product and native application are `Orlix`. The sole public SDK is `OrlixOS.xcframework`, identified by `com.rudironsoni.orlix.os`.

`OrlixKernel.xcframework`, `OrlixMLibC.xcframework`, and `OrlixCoreUtils.xcframework` are private static implementation artifacts identified by `com.rudironsoni.orlix.os.kernel`, `com.rudironsoni.orlix.os.mlibc`, and `com.rudironsoni.orlix.os.coreutils`. They are linked into the delivered product through `OrlixOS` and are not public SDKs. `OrlixHostAdapter` remains private host execution integration. `OrlixTCTI` remains private guest instruction execution under `arch/orlix`.

`OrlixOS` owns its curated distribution resources directly. There is no separate product payload bundle, payload SDK, or target-metadata-selected payload identity.

The public local Linux lifecycle type is `OrlixMachine`. The public container surface is nested under `OrlixOS.Containers`. Docker Engine and Compose compatibility remain the accepted target from [ADR 0028](0028-provide-full-docker-engine-compatibility-through-orlixos.md); the namespace does not narrow that target or claim it is implemented.

Herdr retains its own hierarchy: a Session contains Workspaces, each Workspace contains Tabs, and each Tab owns a split layout of Panes. Orlix presents that hierarchy without defining parallel public topology types or an app-specific Herdr session.

The private app-hosted test applications are `OrlixTerminalTestApp` for the native terminal application and `OrlixOSTestApp` for the OrlixOS Linux userspace and its implementation-layer proof. Neither is a product or public SDK.

Retired public names receive no compatibility aliases. Source, tests, and structured work move to the canonical names instead of preserving duplicate modules, symbols, targets, or type aliases.

## Consequences

Application consumers import only `OrlixOS`. Public lifecycle and container APIs use `OrlixMachine` and `OrlixOS.Containers`; private static artifacts may remain independently buildable and testable without becoming consumer dependencies.

Herdr commercial availability and integration remain unfinished until their owning todo tasks pass. OCI runtime and Docker/Compose behavior remain unfinished until the owning lifecycle and conformance tasks pass. Packaging or naming conformance cannot promote either roadmap gate.
