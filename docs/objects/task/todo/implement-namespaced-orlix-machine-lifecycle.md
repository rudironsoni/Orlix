---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-09-10
status: todo
summary: "Implement namespaced OrlixInstance creation, lifecycle, and terminal attachment."
task_of:
  - "[Deliver Orlix machines](../../story/doing/deliver-orlix-machines.md)"
depends_on:
  - "[Bind OrlixInstance sessions through OrlixOS](../doing/bind-orlix-machine-sessions-through-orlixos.md)"
  - "[Complete the pinned simulator TCTI ladder](../doing/complete-pinned-simulator-tcti-ladder.md)"
blocks:
  - "[Prove concurrent OrlixInstance isolation](prove-concurrent-orlix-machine-isolation.md)"
---

# Implement namespaced OrlixInstance lifecycle

Implement the Linux-shaped namespace and lifecycle contract needed for independently named OrlixInstance systems while keeping the public OrlixKit runtime API instance-shaped and presentation in the app. OrlixEngine remains process-global and hosts one OrlixOS; that constraint must remain explicit and cannot be hidden behind the API.
