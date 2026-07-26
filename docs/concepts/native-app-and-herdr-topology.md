---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-26
summary: "The native app owns Apple presentation while Herdr owns Session, Workspace, Tab, and Pane topology."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Native app and Herdr topology

The native app owns Apple presentation. Herdr is authoritative for its hierarchy: a Session contains Workspaces, each Workspace contains Tabs, and each Tab owns a split layout of Panes. The app renders that state without creating a parallel topology model or hidden app-specific Session.

An `OrlixMachine` or `OrlixOS.Containers` target binds to a Herdr-owned Pane through Linux PTYs. OrlixOS owns the delivered Linux session rather than terminal UI. The commercial Herdr platform and external Pane backend remain unfinished until [Secure Herdr commercial integration](../objects/task/todo/secure-herdr-commercial-integration.md) and [Integrate and validate the Herdr terminal platform](../objects/task/todo/integrate-and-validate-herdr-terminal-platform.md) are done.
