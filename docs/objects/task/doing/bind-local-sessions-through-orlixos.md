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
---

# Bind local sessions through OrlixOS

Connect local environment sessions through bounded, ordered, resumable contracts. OrlixOS retains session and payload ownership, while application presentation retains terminal rendering and interaction ownership.

The current default local session must establish one authoritative terminal geometry before boot, propagate later grid changes to the Linux PTY, and keep Ghostty lifecycle behavior in the native application. The OrlixOS session API remains instance-shaped, but the hosted kernel, boot-progress storage, virtio console input, and active HostAdapter output registration are process-global. This task does not claim independent concurrent kernels or multiple active local console transports.
