---
type: task
tags:
  - task
  - oci, docker
updated: 2026-07-26
status: todo
summary: "Prove Docker engine-facing behavior after the OCI image and runtime boundaries pass."
task_of:
  - "[Deliver OCI environment lifecycle](../../story/doing/deliver-oci-environment-lifecycle.md)"
depends_on:
  - "[Implement OCI runtime lifecycle](implement-oci-runtime-lifecycle.md)"
blocks:
  - "[Validate the mobile container simulator product](validate-mobile-container-simulator-product.md)"
---

# Prove Docker engine compatibility

Exercise the complete Docker Engine and Compose contract accepted by [ADR 0028](../../architecture-decision/0028-provide-full-docker-engine-compatibility-through-orlixos.md) through `OrlixOS.Containers`. Image import, the public namespace, or an incomplete runtime lifecycle cannot satisfy this unfinished task.
