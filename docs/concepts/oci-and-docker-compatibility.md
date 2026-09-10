---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-09-10
summary: "OrlixContainer exposes OCI lifecycle inside an OrlixInstance while Docker and Compose remain unfinished conformance targets."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# OCI and Docker compatibility

`OrlixContainer` is the public OCI container handle and belongs to one `OrlixInstance`. OCI and Docker behavior is built from upstream Linux namespaces, cgroups, networking, storage, process, and lifecycle interfaces rather than a product-specific runtime facade.

[ADR 0040](../objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md) amends [ADR 0028](../objects/architecture-decision/0028-provide-full-docker-engine-compatibility-through-orlixos.md) and retains full Docker Engine and Compose compatibility as a later conformance target. OCI `create`, `start`, `state`, `kill`, `delete`, exec, and wait lifecycle is part of the recovery, but Docker and Compose behavior is unfinished until [Implement OCI runtime lifecycle](../objects/task/todo/implement-oci-runtime-lifecycle.md) and [Prove Docker engine compatibility](../objects/task/todo/prove-docker-engine-compatibility.md) pass.
