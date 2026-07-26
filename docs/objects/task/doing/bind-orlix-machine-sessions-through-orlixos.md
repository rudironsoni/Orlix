---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-26
status: doing
summary: "Bind Herdr panes to OrlixMachine sessions created and delivered by OrlixOS."
task_of:
  - "[Deliver Orlix machines](../../story/doing/deliver-orlix-machines.md)"
applies:
  - "[Terminal transport multiplex protocol](../../../concepts/terminal-transport-multiplex-protocol.md)"
blocks:
  - "[Implement namespaced OrlixMachine lifecycle](../todo/implement-namespaced-orlix-machine-lifecycle.md)"
  - "[Validate mobile platform presentation](../todo/validate-mobile-platform-presentation.md)"
---

# Bind OrlixMachine sessions through OrlixOS

Connect OrlixMachine sessions through bounded, ordered, resumable contracts. OrlixOS retains machine session and curated-resource ownership, while application presentation retains terminal rendering and interaction ownership.

The current default machine session must establish one authoritative terminal geometry before boot, send it as the first event through the same multiplex transport used for later resizes, and keep Ghostty lifecycle behavior in the native application. OrlixOS derives the app-facing console source from the last Linux `console=` boot-policy token and fails when that selected source is missing or unsupported. The OrlixMachine API is machine-shaped, but the hosted kernel, boot-progress storage, console input, and active HostAdapter output registration remain process-global. This task does not claim independent concurrent kernels or multiple active local console transports.
