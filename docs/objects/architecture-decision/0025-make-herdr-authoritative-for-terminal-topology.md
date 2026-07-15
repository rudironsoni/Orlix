---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-15
status: accepted
external_id: "ADR-0025"
summary: "Durable Orlix architecture decision ADR 0025."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# ADR 0025: Make Herdr Authoritative For Terminal Topology

## Status

Accepted, subject to a commercial Herdr license and App Store approval.

## Context

Orlix needs Herdr as a first-class multiplexer inside the Orlix Linux distribution, as a native macOS executable, through the raw TUI, and through the native Orlix application. If Orlix and Herdr each own Workspaces, Tabs, splits, or Pane lifecycle, detach, reconnect, restoration, agents, and user-created sessions will diverge.

Herdr is AGPL-3.0-or-later or commercially licensed. Its full product surface includes agents, status rollups, Workspaces, Tabs, Panes, default and named Sessions, persistence, worktrees, plugins, integrations, marketplace behavior, raw TUI, CLI, socket API, remote attach, mouse behavior, and keyboard bindings.

## Decision

Obtain a commercial Herdr license and make the Herdr server authoritative for Workspace, Tab, split-layout, Pane, focus, process, terminal, and agent state. The native Orlix UI and raw Herdr TUI are peer clients of the same server state.

Orlix follows Herdr terminology and lifecycle exactly:

- A Workspace is the top-level project container and is normally used per repo, task, or investigation.
- A Tab is a layout inside a Workspace.
- A Pane is a real terminal owned by the Herdr server.
- A Session is a persistent Herdr server namespace.
- The default `herdr` command attaches to the default Session.
- Named Sessions have separate Panes, sockets, and persisted runtime state.
- Workspaces are used before named Sessions. Named Sessions are for complete runtime separation.

Orlix creates the migrated initial Workspace in the default Session. It does not create a hidden app-specific Session named `orlix`. App-created and user-created Workspaces, Tabs, and Panes are ordinary Herdr resources with equal lifecycle rights. Orlix lists named Sessions without merging them into the default Session.

Local Instance and Container targets use Herdr-owned Linux PTYs. App-native SSH, Mosh, and TSSH transports use a commercially licensed external Pane backend extension developed with the Herdr maintainer. The extension preserves ordered input and output, resize ordering, bounded backpressure, controller ownership, takeover, detach, reconnect, terminal-mode restoration, explicit closed/error states, raw TUI compatibility, and protocol-version negotiation.

Native clients bootstrap with `session.snapshot`, subscribe to public events, and resnapshot after reconnects, revision gaps, or protocol mismatches. Orlix does not read Herdr private state or implement a parallel split tree.

## Consequences

Herdr must run correctly on Orlix Linux with real PTYs, process groups, signals, polling, Unix sockets, process inspection, persistence, and terminal semantics. Missing Linux behavior is fixed in upstream Linux port inputs or OrlixMLibC, not hidden in Swift or OrlixHostAdapter.

The complete Herdr surface, including plugins, integrations, and marketplace behavior, is a first-release gate. If the commercial agreement, upstream extension, executable-content policy, or App Review process does not permit that surface, the public terminal release stops rather than shipping an Orlix-specific substitute.
