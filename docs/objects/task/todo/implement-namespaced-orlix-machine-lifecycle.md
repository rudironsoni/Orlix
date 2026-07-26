---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-26
status: todo
summary: "Implement namespaced OrlixMachine creation, lifecycle, and terminal attachment."
task_of:
  - "[Deliver Orlix machines](../../story/doing/deliver-orlix-machines.md)"
depends_on:
  - "[Bind OrlixMachine sessions through OrlixOS](../doing/bind-orlix-machine-sessions-through-orlixos.md)"
  - "[Complete the pinned simulator TCTI ladder](../doing/complete-pinned-simulator-tcti-ladder.md)"
blocks:
  - "[Prove concurrent OrlixMachine isolation](prove-concurrent-orlix-machine-isolation.md)"
---

# Implement namespaced OrlixMachine lifecycle

Implement the smallest Linux-shaped namespace and lifecycle contract needed for independently named OrlixMachine systems while keeping the public OrlixOS session API machine-shaped and presentation in the app. Process-global hosted-kernel limitations must remain explicit and cannot be hidden behind the API.
