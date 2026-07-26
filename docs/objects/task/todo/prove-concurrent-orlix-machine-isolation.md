---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-26
status: todo
summary: "Prove concurrent OrlixMachine systems preserve independent lifecycle, terminal, and Linux-visible state."
task_of:
  - "[Deliver Orlix machines](../../story/doing/deliver-orlix-machines.md)"
depends_on:
  - "[Implement namespaced OrlixMachine lifecycle](implement-namespaced-orlix-machine-lifecycle.md)"
blocks:
  - "[Implement OCI runtime lifecycle](implement-oci-runtime-lifecycle.md)"
  - "[Validate the mobile terminal simulator product](../doing/validate-mobile-terminal-simulator-product.md)"
---

# Prove concurrent OrlixMachine isolation

Demonstrate that concurrent OrlixMachine systems preserve independent session lifetime, terminal attachment, namespace-visible state, and failure behavior. This proof remains unfinished and is required before OCI lifecycle or the mobile terminal product can claim machine isolation.
