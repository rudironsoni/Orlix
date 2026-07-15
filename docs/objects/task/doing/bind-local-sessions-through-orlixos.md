---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-15
status: doing
summary: "Bind Herdr panes to local Linux sessions created and delivered by OrlixOS."
task_of:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
applies:
  - "[Terminal transport multiplex protocol](../../../concepts/terminal-transport-multiplex-protocol.md)"
---

# Bind local sessions through OrlixOS

Connect local environment sessions through bounded, ordered, resumable contracts. OrlixOS retains session and payload ownership, while application presentation retains terminal rendering and interaction ownership.

The current default local session must establish one authoritative terminal geometry before boot, send it as the first event through the same multiplex transport used for later resizes, and keep Ghostty lifecycle behavior in the native application. OrlixOS derives the app-facing console source from the last Linux `console=` boot-policy token and fails when that selected source is missing or unsupported. The OrlixOS session API remains instance-shaped, but the hosted kernel, boot-progress storage, console input, and active HostAdapter output registration are process-global. This task does not claim independent concurrent kernels or multiple active local console transports.
