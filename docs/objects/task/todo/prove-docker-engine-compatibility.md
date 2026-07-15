---
type: task
tags:
  - task
  - oci, docker
updated: 2026-07-15
status: todo
summary: "Prove Docker engine-facing behavior after the OCI image and runtime boundaries pass."
task_of:
  - "[Deliver OCI environment lifecycle](../../story/doing/deliver-oci-environment-lifecycle.md)"
---

# Prove Docker engine compatibility

Exercise the engine-facing contract accepted by [ADR 0028](../../architecture-decision/0028-provide-full-docker-engine-compatibility-through-orlixos.md). Image import or an incomplete runtime lifecycle cannot satisfy this task.
