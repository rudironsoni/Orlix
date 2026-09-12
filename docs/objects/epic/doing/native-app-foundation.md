---
type: epic
tags:
  - epic
  - native-app-foundation
updated: 2026-09-10
status: doing
summary: "Maintain the native Apple application foundation and Herdr-owned terminal topology."
targets:
  - "[Orlix](../../product/orlix.md)"
has_story:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
  - "[Deliver Orlix machines](../../story/doing/deliver-orlix-machines.md)"
blocks:
  - "[OCI-derived environments](oci-derived-environments.md)"
  - "[Orlix release](orlix-release.md)"
---

# Native app foundation

Maintain the native Apple application foundation and Herdr-owned terminal topology.

The product application uses native SwiftUI and platform adapters for Apple presentation. Shared feature UI remains platform-neutral where practical, while iOS, iPadOS, and macOS own their native window, toolbar, keyboard, selection, command, and lifecycle integration. Feature code is organized by product capability. The vvterm-derived terminal surface remains in Orlix.app and may become an independently buildable internal app module when actual dependencies support that boundary; it is not moved into OrlixKit.

Herdr is authoritative for Session, Workspace, Tab, Pane, focus, topology, and raw terminal compatibility. Remote SSH, OrlixInstance, and OrlixContainer targets bind external terminal backends to Herdr panes through bounded, ordered, resumable transport contracts. OrlixKit owns the local runtime entry point, OrlixOS is the running OS, and OrlixDistribution owns guest resource assembly. The app must not duplicate topology or move OS delivery into presentation code. Commercial Herdr availability and integration remain unfinished under their owning todo tasks.

The first public product keeps Apple-platform integration App Store compatible. Hardware identities, network extensions, resumable transports, and external services require explicit platform-safe contracts. Current implementation state belongs to source, tests, and structured reports rather than this epic page.
