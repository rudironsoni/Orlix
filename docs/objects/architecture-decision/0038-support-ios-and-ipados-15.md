---
type: architecture-decision
tags: [architecture, decision, ios, compatibility]
updated: 2026-09-01
status: accepted
external_id: "ADR-0038"
summary: "Support iOS and iPadOS 15.0 while keeping newer optional Apple features behind availability boundaries."
part_of:
  - "[Orlix](../product/orlix.md)"
amends:
  - "[ADR 0024](0024-adopt-orlix-native-application-foundation.md)"
  - "[ADR 0027](0027-use-app-store-only-cross-platform-host-integration.md)"
---

# ADR 0038: Support iOS And iPadOS 15

## Status

Accepted.

## Context

The Orlix app and its sole public SDK must run on iOS and iPadOS 15. Existing decisions selected 16.1. ActivityKit and Live Activities require 16.1, but that optional feature must not raise the minimum version of the app or public SDK.

## Decision

Set the Orlix app, test app, private libraries, and public `OrlixOS.xcframework` minimum to iOS and iPadOS 15.0. Keep Apple compilation mode and the Orlix `release` or `development` profile independent from this deployment target.

Every Swift package and Orlix product target must compile with the iOS 15 deployment target. Pin the reviewed MLXSwift compatibility fork that replaces its iOS 16-only Swift and Metal compile requirements. MLX execution remains behind the existing iOS 16 availability boundary, and Apple Speech remains the iOS 15 transcription provider. A successful compile does not count as runtime proof on iOS 15.

Keep `OrlixLiveActivity` as an optional embedded extension with an independent iOS and iPadOS 16.1 minimum. Protect every call and shared type that uses ActivityKit with the existing availability checks. The app must remain usable on iOS 15 without loading or invoking the extension.

The iOS 15 product is a supported reduced feature set:

| Capability | iOS 15 behavior | Newer-system behavior |
| --- | --- | --- |
| SSH, terminal, remote files, settings, and OrlixOS session surfaces | Supported | Supported |
| Navigation, sheets, charts, and responsive dashboard layout | Compatibility presentation with the same user data and actions | Native newer SwiftUI presentation |
| Voice transcription | Apple Speech | Apple Speech or MLX on supported iOS 16 or later systems |
| Live Activities | Not available | Available on iOS 16.1 or later |
| File-browser toolbar spacing | Standard bottom toolbar | iOS 26 toolbar spacing and presentation |

Do not delete a newer feature to make iOS 15 compile. Keep the feature behind
an availability or capability gate, and provide a smaller iOS 15 presentation
or an explicit unavailable state. The gate must prevent unavailable frameworks
and APIs from loading or executing on iOS 15.

A dependency that requires a later OS version cannot raise the application minimum. Isolate, replace, or patch that dependency under its owning source and license policy. Record the exact dependency change and prove the iOS 15 link and launch result.

## Consequences

Every canonical Apple build records the 15.0 application and package deployment target and the 16.1 Live Activity deployment target. The Bazel feasibility gate, Xcode project generation, simulator proof, archive inspection, and release manifest must check these values.

Source-level availability checks are not runtime proof. Orlix cannot claim iOS 15 support until the app compiles, links, installs, launches, and completes the required product proof on an iOS 15 destination. When a developer host lacks the runtime, the protected GitHub CI lane installs iOS 15.5 with pinned `xcodes` and owns that proof. Missing local runtime state cannot reduce the support contract.
