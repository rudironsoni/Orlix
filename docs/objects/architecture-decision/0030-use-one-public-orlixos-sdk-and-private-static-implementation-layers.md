---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-09-10
status: accepted
external_id: "ADR-0030"
summary: "Keep OrlixKit as the public embeddable SDK while keeping native implementation and guest distribution layers private."
part_of:
  - "[Orlix](../product/orlix.md)"
amended_by:
  - "[ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md)"
---

# ADR 0030: Keep One Public OrlixKit SDK And Private Implementation Layers

## Status

Accepted.

## Context

Orlix needs one stable product and SDK vocabulary. Separate public kernel, libc, command, payload, test-host, or runtime-facade identities would expose implementation layers as competing products and make application code depend on packaging details.

## Decision

The product and native application are `Orlix`. The public embeddable SDK is `OrlixKit.xcframework`.

`OrlixKernel.xcframework` and the OrlixBootloader and OrlixHostAdapter targets are private Apple-native implementation artifacts. OrlixMLibC, OrlixCoreUtils, guest packages, and rootfs/images are private Linux guest or distribution artifacts. They are packaged or referenced by OrlixKit as required, but guest artifacts are not Apple-native link dependencies. OrlixTCTI remains private guest instruction execution under `arch/orlix`.

OrlixDistribution is the internal build and packaging concept for curated guest resources. OrlixOS is the running hosted Linux operating system. There is no target-metadata-selected mutable payload identity.

The public local Linux lifecycle types are `OrlixEngine`, `OrlixOS`, `OrlixInstance`, `OrlixProcess`, and `OrlixContainer`. Docker Engine and Compose compatibility remain the accepted target from [ADR 0028](0028-provide-full-docker-engine-compatibility-through-orlixos.md); the public shape does not claim that conformance is implemented.

Herdr retains its own hierarchy: a Session contains Workspaces, each Workspace contains Tabs, and each Tab owns a split layout of Panes. Orlix presents that hierarchy without defining parallel public topology types or an app-specific Herdr session.

The private app-hosted test applications are `OrlixTestApp` for lower implementation-layer proof and `OrlixOSTestApp` for the public OrlixKit product-session path, including a running OrlixOS. Neither is a product or public SDK.

Retired public names receive no compatibility aliases. Source, tests, and structured work move to the canonical names instead of preserving duplicate modules, symbols, targets, or type aliases.

## Consequences

Application consumers import only OrlixKit for the local Linux runtime. Public lifecycle APIs use OrlixEngine, OrlixOS, OrlixInstance, OrlixProcess, and OrlixContainer. Private native targets and guest distribution artifacts may remain independently buildable and testable without becoming direct consumer dependencies. The current source graph remains under migration until the recovery gates pass.

Herdr commercial availability and integration remain unfinished until their owning todo tasks pass. OCI runtime and Docker/Compose behavior remain unfinished until the owning lifecycle and conformance tasks pass. Packaging or naming conformance cannot promote either roadmap gate.
