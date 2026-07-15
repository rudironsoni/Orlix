---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-15
status: todo
summary: "Implement instance-scoped Local Instance creation, lifecycle, and terminal attachment."
task_of:
  - "[Deliver Local Runtime and Local Instances](../../story/doing/deliver-local-runtime-and-instances.md)"
depends_on:
  - "[Bind local sessions through OrlixOS](../doing/bind-local-sessions-through-orlixos.md)"
  - "[Complete the pinned simulator TCTI ladder](../doing/complete-pinned-simulator-tcti-ladder.md)"
blocks:
  - "[Prove concurrent Local Instance isolation](prove-concurrent-local-instance-isolation.md)"
---

# Implement namespaced Local Instance lifecycle

Implement the smallest Linux-shaped namespace and lifecycle contract needed for independently named Local Instances while keeping OrlixOS session APIs instance-scoped and presentation in the app. Process-global hosted-kernel limitations must remain explicit and cannot be hidden behind an instance-shaped API.
