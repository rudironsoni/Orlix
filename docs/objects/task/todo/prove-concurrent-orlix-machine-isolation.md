---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-09-10
status: todo
summary: "Prove concurrent OrlixInstance systems preserve independent lifecycle, process I/O, and Linux-visible state."
task_of:
  - "[Deliver Orlix machines](../../story/doing/deliver-orlix-machines.md)"
depends_on:
  - "[Implement namespaced OrlixInstance lifecycle](implement-namespaced-orlix-machine-lifecycle.md)"
blocks:
  - "[Implement OCI runtime lifecycle](implement-oci-runtime-lifecycle.md)"
  - "[Validate the mobile terminal simulator product](../doing/validate-mobile-terminal-simulator-product.md)"
---

# Prove concurrent OrlixInstance isolation

Demonstrate that concurrent OrlixInstance systems share one OrlixOS and Kernel while preserving independent session lifetime, process and PTY attachment, PID/mount/UTS/network/user/IPC/root/cgroup state, and failure behavior. Functional external networking is claimed only to the proof tier established by the current network implementation. This proof remains unfinished and is required before OCI lifecycle or the mobile terminal product can claim instance isolation.
