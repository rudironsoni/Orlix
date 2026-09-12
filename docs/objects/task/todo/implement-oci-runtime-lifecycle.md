---
type: task
tags:
  - task
  - oci
updated: 2026-09-10
status: todo
summary: "Map OCI process configuration and lifecycle operations onto normal Orlix Linux mechanisms inside OrlixInstances."
task_of:
  - "[Deliver OCI environment lifecycle](../../story/doing/deliver-oci-environment-lifecycle.md)"
depends_on:
  - "[Import OCI image content](../doing/import-oci-image-content.md)"
  - "[Prove concurrent OrlixInstance isolation](prove-concurrent-orlix-machine-isolation.md)"
blocks:
  - "[Prove Docker engine compatibility](prove-docker-engine-compatibility.md)"
---

# Implement OCI runtime lifecycle

The public lifecycle belongs under OrlixKit as `OrlixContainer`, and each container belongs to one `OrlixInstance`. This todo task remains the prerequisite for any Docker or Compose implementation claim.

Implement and prove `create`, `start`, `state`, `kill`, `delete`, process exec, and wait through OrlixKit policy, OrlixKernel Linux behavior, and narrow private host mechanics according to their ownership boundaries.
