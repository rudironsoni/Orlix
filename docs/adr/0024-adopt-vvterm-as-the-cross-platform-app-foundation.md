# ADR 0024: Adopt VVTerm As The Cross-Platform App Foundation

## Status

Accepted, subject to the legal gate below.

## Context

The current Orlix host app is an iOS-only UIKit terminal surface with direct Ghostty-to-OrlixOS session wiring. Orlix needs a native iOS, iPadOS, and macOS application with complete terminal, connection, remote-file, sync, security, customization, StoreKit, restoration, and Apple platform behavior. VVTerm already provides that cross-platform application shape, while RootShell, Orchard, and Contained provide separate behavioral, structural, and visual references.

VVTerm source is GPL-3.0. Its separate App Store EULA applies to official VVTerm binaries and is not assumed to grant Orlix distribution rights. Combining a GPL-derived app with paid App Store distribution is a legal and distribution decision, not merely an engineering dependency choice.

## Decision

Import pinned VVTerm revision `791eebae946b0831ffff3ac839e0f2b75d076458` with preserved history and compile its application source directly as the Orlix iOS and iPadOS application. Preserve VVTerm's recognizable `App`, `Core`, `Features`, `GhosttyTerminal`, `Compatibility`, `Generated`, and `Resources` organization and its full implemented feature surface through an explicit parity ledger. Do not create a reusable `OrlixTerminal` module, generic product boundary, or rewrite based on the current terminal controller. Orlix remains the app identity, VVTerm's SwiftUI application root becomes the product composition root, OrlixOS remains the delivered Linux Kit, and Ghostty remains presentation rather than Linux runtime authority.

The first public replacement supports iOS and iPadOS 16.1 or later. It preserves the Orlix bundle identifier and existing preferences. It does not migrate data, credentials, purchases, or StoreKit entitlement from the separate VVTerm app. Native Apple-silicon macOS 13.3 or later is implemented only after the mobile terminal and container releases are in good shape and published to the App Store. The direct import retains VVTerm's macOS-compatible source, resources, package declarations, and conditional compilation so mobile work lays that foundation without starting the Mac product early.

Written legal approval is required before public distribution. Approval must cover GPL obligations, App Store terms, paid products, corresponding-source availability, notices, binary distribution, and every statically linked copyleft dependency. Orlix publishes the required source, provenance, modification notices, and license texts. If counsel does not approve the combined distribution model, public release stops. The implementation must not silently reinterpret VVTerm's App Store EULA or remove source obligations.

RootShell remains a behavioral and regression reference only. Contained remains a visual reference only because its PolyForm Noncommercial license is incompatible with the intended commercial product. Orchard's MIT code may inform typed application patterns, but Apple Container dependencies do not enter the Orlix runtime.

## Consequences

`TerminalViewController` remains an internal, developer-only emergency fallback until the VVTerm-derived mobile surface passes terminal, transport, Herdr, migration, restoration, accessibility, localization, and App Store gates. Because it uses `libghostty-spm` while VVTerm vendors its own Ghostty build, isolate the fallback in a separate developer-only diagnostic app target. The production Orlix target has one SwiftUI `@main`, supplied by the VVTerm-derived application, and does not link or compile the legacy controller. The fallback is not refactored, expanded, or exposed through normal product navigation, and it is removed only at the verified mobile cutover.

The Orlix mobile target becomes the VVTerm-derived application directly. App-facing Linux lifecycle and payload behavior continue to flow through OrlixOS. VVTerm data and purchase migration remain explicitly out of scope.
