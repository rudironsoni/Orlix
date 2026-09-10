---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-09-10
status: accepted
external_id: "ADR-0024"
summary: "Durable Orlix architecture decision ADR 0024."
part_of:
  - "[Orlix](../product/orlix.md)"
supersedes:
  - "[ADR 0015](0015-build-orlix-as-the-ios-host-app.md)"
amended_by:
  - "[ADR 0032](0032-sync-vvterm-with-three-way-source-snapshots.md)"
  - "[ADR 0038](0038-support-ios-and-ipados-15.md)"
  - "[ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md)"
---

# ADR 0024: Adopt Orlix As The Native Application Foundation

## Status

Accepted, subject to the legal gate below.

## Context

Orlix needs a native iOS, iPadOS, and later macOS application with complete terminal, connection, remote-file, sync, security, customization, StoreKit, restoration, and Apple platform behavior. Orlix uses the pinned upstream source revision as the ancestry for its own application fork, while RootShell, Orchard, and Contained provide separate behavioral, structural, and visual references.

The forked source is GPL-3.0. Combining a GPL-derived app with paid App Store distribution is a legal and distribution decision, not merely an engineering dependency choice. Immutable origin and artifact details remain in the Orlix application source-provenance record.

## Decision

Maintain the pinned revision `791eebae946b0831ffff3ac839e0f2b75d076458` as the source ancestry of the Orlix-owned application fork. Compile that fork directly as the Orlix iOS and iPadOS application. Preserve its `App`, `Core`, `Features`, `GhosttyTerminal`, `Compatibility`, `Generated`, and `Resources` organization and full implemented feature surface through an explicit parity ledger. The terminal and vvterm-derived surface remain in Orlix.app, but may be independently buildable as an internal app module when actual dependencies support that boundary. `Orlix` is the sole app identity and product composition root, OrlixKit is the public SDK, and Ghostty remains presentation rather than Linux runtime authority. The upstream name is not a product name, compatibility name, target, module, bundle identifier, UI label, source directory, or public architecture concept in Orlix. It may appear only where immutable upstream provenance or legally required attribution must identify the original work accurately.

The first public replacement supports iOS and iPadOS 15.0 or later, as amended by ADR 0038. The optional Live Activity extension requires iOS and iPadOS 16.1 or later. The app preserves the Orlix bundle identifier and existing preferences. Native Apple-silicon macOS 13.3 or later is implemented only after the mobile terminal and container releases are in good shape and published to the App Store. The application retains macOS-compatible source, resources, package declarations, and conditional compilation so mobile work lays that foundation without starting the Mac product early.

Written legal approval is required before public distribution. Approval must cover GPL obligations, App Store terms, paid products, corresponding-source availability, notices, binary distribution, and every statically linked copyleft dependency. Orlix publishes the required source, provenance, modification notices, and license texts. If counsel does not approve the combined distribution model, public release stops.

RootShell remains a behavioral and regression reference only. Contained remains a visual reference only because its PolyForm Noncommercial license is incompatible with the intended commercial product. Orchard's MIT code may inform typed application patterns, but Apple Container dependencies do not enter the Orlix runtime.

## Consequences

The production Orlix target has exactly one SwiftUI `@main`, supplied by the `Orlix` entry point in `Orlix/Orlix/App/Orlix.swift`, and directly compiles the complete native application. The retired UIKit prototype, its separate application target, duplicate assets, and conflicting Ghostty package are removed rather than retained as a fallback.

App-facing Linux lifecycle and payload behavior continue to flow through OrlixOS. Application-data and purchase migration from unrelated products remains explicitly out of scope.
