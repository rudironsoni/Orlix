---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-15
status: todo
summary: "Integrate the approved Herdr terminal platform with Orlix Linux and validate its product contract."
task_of:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
depends_on:
  - "[Secure Herdr commercial integration](secure-herdr-commercial-integration.md)"
  - "[Complete the pinned simulator TCTI ladder](../doing/complete-pinned-simulator-tcti-ladder.md)"
blocks:
  - "[Keep Herdr authoritative for terminal topology](../doing/keep-herdr-authoritative-for-terminal-topology.md)"
---

# Integrate and validate the Herdr terminal platform

Port and validate the Herdr server, CLI, and raw TUI behavior through normal Orlix Linux userspace, then connect the approved external pane backend without moving topology ownership into OrlixOS. Use upstream behavior, focused fault coverage, and the pinned app-hosted execution path as the acceptance surfaces.
