---
type: task
tags:
  - task
  - oci
updated: 2026-07-15
status: todo
summary: "Map OCI process configuration and lifecycle operations onto normal Orlix Linux mechanisms."
task_of:
  - "[Deliver OCI environment lifecycle](../../story/doing/deliver-oci-environment-lifecycle.md)"
depends_on:
  - "[Import OCI image content](../doing/import-oci-image-content.md)"
  - "[Prove concurrent Local Instance isolation](prove-concurrent-local-instance-isolation.md)"
blocks:
  - "[Prove Docker engine compatibility](prove-docker-engine-compatibility.md)"
---

# Implement OCI runtime lifecycle

Implement and prove `create`, `start`, `state`, `kill`, and `delete` through OrlixOS policy, OrlixKernel Linux behavior, and narrow private host mechanics according to their ownership boundaries.
