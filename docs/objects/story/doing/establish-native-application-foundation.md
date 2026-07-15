---
type: story
tags:
  - story
  - native-app-foundation
updated: 2026-07-15
status: doing
summary: "Provide native Apple presentation over Herdr-owned terminal topology and OrlixOS sessions."
story_of:
  - "[Native app foundation](../../epic/doing/native-app-foundation.md)"
has_task:
  - "[Keep Herdr authoritative for terminal topology](../../task/doing/keep-herdr-authoritative-for-terminal-topology.md)"
  - "[Bind local sessions through OrlixOS](../../task/todo/bind-local-sessions-through-orlixos.md)"
  - "[Validate native platform presentation](../../task/todo/validate-native-platform-presentation.md)"
---

# Establish native application foundation

As an Orlix user, I want native Apple-platform presentation over one authoritative terminal topology so remote and local sessions behave consistently without duplicating OS or terminal ownership.

The story is complete when Herdr owns workspace, tab, pane, and focus topology; OrlixOS owns local Linux session construction and payload delivery; and iOS, iPadOS, and macOS adapters pass their focused presentation tests.
