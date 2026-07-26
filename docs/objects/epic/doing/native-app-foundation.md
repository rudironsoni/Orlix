---
type: epic
tags:
  - epic
  - native-app-foundation
updated: 2026-07-26
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

The product application uses native SwiftUI and platform adapters for Apple presentation. Shared feature UI remains platform-neutral where practical, while iOS, iPadOS, and macOS own their native window, toolbar, keyboard, selection, command, and lifecycle integration. Feature code is organized by product capability rather than a second reusable terminal framework.

Herdr is authoritative for Session, Workspace, Tab, Pane, focus, topology, and raw terminal compatibility. Remote SSH, OrlixMachine, and future `OrlixOS.Containers` targets bind external terminal backends to Herdr panes through bounded, ordered, resumable transport contracts. OrlixOS owns local Linux session construction and curated distribution resources. The app must not duplicate topology or move OS delivery into presentation code. Commercial Herdr availability and integration remain unfinished under their owning todo tasks.

The first public product keeps Apple-platform integration App Store compatible. Hardware identities, network extensions, resumable transports, and external services require explicit platform-safe contracts. Current implementation state belongs to source, tests, and structured reports rather than this epic page.
