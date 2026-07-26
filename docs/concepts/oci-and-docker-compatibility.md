---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-26
summary: "OrlixOS.Containers exposes OCI lifecycle while Docker and Compose compatibility remain unfinished conformance targets."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# OCI and Docker compatibility

`OrlixOS.Containers` is the public container namespace. OCI and Docker behavior is built from upstream Linux namespaces, cgroups, networking, storage, process, and lifecycle interfaces rather than a product-specific runtime facade.

[ADR 0028](../objects/architecture-decision/0028-provide-full-docker-engine-compatibility-through-orlixos.md) retains full Docker Engine and Compose compatibility as the target. That behavior is unfinished until [Implement OCI runtime lifecycle](../objects/task/todo/implement-oci-runtime-lifecycle.md) and [Prove Docker engine compatibility](../objects/task/todo/prove-docker-engine-compatibility.md) pass; the namespace and API shape are not implementation or conformance evidence.
