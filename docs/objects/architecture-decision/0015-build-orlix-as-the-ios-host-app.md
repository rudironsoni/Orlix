---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-15
status: superseded
external_id: "ADR-0015"
summary: "Durable Orlix architecture decision ADR 0015."
part_of:
  - "[Orlix](../product/orlix.md)"
superseded_by:
  - "[ADR 0024](0024-adopt-orlix-native-application-foundation.md)"
---

# ADR 0015: Build Orlix As The iOS Host App

## Status

Superseded by ADR 0024

## Context

The iOS host app could be a blank XCTest host, a diagnostic harness, or the actual terminal-shaped app that users will recognize. Orlix needs iOS-hosted proof, but the product direction is Linux inside an iOS app with terminal interaction.

The first host-app prototype used a UIKit terminal controller. The production direction now requires the complete native Orlix application surface on iOS and iPadOS.

## Decision

Create the iOS host app as `Orlix` rather than a blank proof-only host. ADR 0024 supersedes the prototype layout: production sources and tests live under `Orlix/App`, terminal presentation uses the vendored Ghostty integration, and `OrlixOS` remains the delivered OS session and payload surface. Do not use a sandbox shell as the execution backend. Orlix owns terminal bytes through Linux console and terminal plumbing.

## Consequences

`Orlix` is the iOS app that consumes `OrlixOS`, embeds the required frameworks, and launches the delivered OrlixOS session for proof and product development.

The retired UIKit prototype is not retained as a target or fallback. The production SwiftUI application is the only Orlix application target.

Before the Linux console path exists, `Orlix` may show non-interactive Orlix boot/proof logs. It must not use a fake shell, sandbox shell, or local execution backend to simulate product behavior.

Linux test output collection is separate from the interactive terminal byte stream. The terminal may display logs when available, but XCTest proof privately captures KUnit output from the kernel log path and kselftest output from test-initramfs stdout.

XCTest should target `Orlix` for iOS-hosted Orlix launch, Linux test-output collection, and terminal/host integration proof. Test code belongs under the owning project `Tests` tree.

The terminal app UI must not become a public Linux management API or OS delivery layer. Linux behavior still belongs inside Orlix Linux, the delivered OS/session surface belongs to `OrlixOS`, and the app acts as host, terminal surface, and proof harness.
