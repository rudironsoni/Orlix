---
type: story
tags:
  - story
  - oci
updated: 2026-07-26
status: doing
summary: "Import OCI images and run their processes through a complete Linux lifecycle."
story_of:
  - "[OCI-derived environments](../../epic/doing/oci-derived-environments.md)"
has_task:
  - "[Import OCI image content](../../task/doing/import-oci-image-content.md)"
  - "[Implement OCI runtime lifecycle](../../task/todo/implement-oci-runtime-lifecycle.md)"
  - "[Prove Docker engine compatibility](../../task/todo/prove-docker-engine-compatibility.md)"
depends_on:
  - "[Deliver Orlix machines](deliver-orlix-machines.md)"
blocks:
  - "[Validate and publish the mobile container release](../todo/validate-and-publish-mobile-container-release.md)"
---

# Deliver OCI environment lifecycle

As an Orlix user, I want OCI-derived environments to behave like Linux environments so image content, writable state, process configuration, and lifecycle operations work through normal OrlixKernel interfaces.

Image import proves only the OCI Image Spec boundary. Completion also requires `create`, `start`, `state`, `kill`, and `delete` behavior, followed by separate engine-facing compatibility proof.
