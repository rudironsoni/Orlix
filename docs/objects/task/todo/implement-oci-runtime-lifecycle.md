---
type: task
tags:
  - task
  - oci
updated: 2026-07-26
status: todo
summary: "Map OCI process configuration and lifecycle operations onto normal Orlix Linux mechanisms."
task_of:
  - "[Deliver OCI environment lifecycle](../../story/doing/deliver-oci-environment-lifecycle.md)"
depends_on:
  - "[Import OCI image content](../doing/import-oci-image-content.md)"
  - "[Prove concurrent OrlixMachine isolation](prove-concurrent-orlix-machine-isolation.md)"
blocks:
  - "[Prove Docker engine compatibility](prove-docker-engine-compatibility.md)"
---

# Implement OCI runtime lifecycle

The public lifecycle belongs under `OrlixOS.Containers`. This todo task remains the prerequisite for any Docker or Compose implementation claim.

Implement and prove `create`, `start`, `state`, `kill`, and `delete` through OrlixOS policy, OrlixKernel Linux behavior, and narrow private host mechanics according to their ownership boundaries.
