---
type: epic
tags:
  - epic
  - oci-derived-environments
updated: 2026-07-15
status: doing
summary: "Deliver OCI-derived Linux environments over upstream kernel and virtio interfaces."
targets:
  - "[Orlix](../../product/orlix.md)"
has_story:
  - "[Deliver OCI environment lifecycle](../../story/doing/deliver-oci-environment-lifecycle.md)"
depends_on:
  - "[Native app foundation](native-app-foundation.md)"
  - "[Orlix TCTI](orlix-tcti.md)"
blocks:
  - "[Orlix release](orlix-release.md)"
---

# OCI-derived environments

Deliver OCI-derived Linux environments over upstream kernel and virtio interfaces.

OrlixOS imports OCI image layouts and root filesystem archives, resolves descriptors, materializes immutable base content with persistent writable state, and constructs named sessions. OrlixKernel supplies normal Linux mounts, OverlayFS, namespaces, cgroups, processes, networking, pseudo-filesystems, device nodes, and execution. OrlixHostAdapter supplies only private backing resources and virtio transport mechanics.

Compatibility work is divided by contract. OCI Image Spec proof covers import and whiteouts. OCI Runtime Spec proof covers process configuration and the create, start, state, kill, and delete lifecycle. Docker compatibility additionally requires the engine-facing behavior accepted by [ADR 0028](../../architecture-decision/0028-provide-full-docker-engine-compatibility-through-orlixos.md). Image import alone cannot establish runtime or Docker compatibility.

Durable fixes belong in OrlixOS inputs, OrlixKernel port inputs, OrlixMLibC sysdeps, or the private host adapter according to ownership. Generated root filesystems, imported sources, build trees, and reports are read-only. Current frontier, Linux-oracle comparisons, app-hosted evidence, and missing gates belong to structured reports under `Build/AgentHarness/`.
