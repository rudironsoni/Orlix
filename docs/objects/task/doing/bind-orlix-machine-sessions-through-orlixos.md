---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-09-10
status: doing
summary: "Bind Herdr panes to OrlixInstance sessions created through OrlixKit and hosted by OrlixOS."
task_of:
  - "[Deliver Orlix instances](../../story/doing/deliver-orlix-machines.md)"
applies:
  - "[Terminal transport multiplex protocol](../../../concepts/terminal-transport-multiplex-protocol.md)"
blocks:
  - "[Implement namespaced OrlixInstance lifecycle](../todo/implement-namespaced-orlix-machine-lifecycle.md)"
  - "[Validate mobile platform presentation](../todo/validate-mobile-platform-presentation.md)"
---

# Bind OrlixInstance sessions through OrlixOS

Connect OrlixInstance sessions through bounded, ordered, resumable contracts. OrlixKit exposes the local runtime boundary, OrlixOS hosts the running Linux system, and application presentation retains terminal rendering and interaction ownership.

The current default instance session must establish one authoritative terminal geometry before attaching a PTY, send it as the first event through the same multiplex transport used for later resizes, and keep Ghostty lifecycle behavior in the native application. OrlixOS derives the app-facing console source from the last Linux `console=` boot-policy token and fails when that selected source is missing or unsupported. OrlixEngine boot state remains process-global by design; instance and process streams must remain independently identifiable. This task does not claim independent kernels or runtime completion before its owning proof passes.
