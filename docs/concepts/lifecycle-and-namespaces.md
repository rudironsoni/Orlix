---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-09-10
summary: "Each OrlixInstance is persistent namespaced Linux userspace hosted by one upstream Linux kernel inside OrlixOS."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Lifecycle and namespaces

One running `OrlixOS` uses one upstream Linux kernel and may host multiple persistent `OrlixInstance` systems using Linux namespaces and cgroups. Each instance owns its userspace lifecycle, root, and namespace identity without pretending to be a separate kernel or VM. Apple application lifecycle maps onto Linux suspend and hibernation without replacing Linux task semantics.

`OrlixEngine` owns process-wide boot and hosts one OrlixOS. `OrlixInstance` is the public lifecycle type for persistent Linux userspace. [ADR 0040](../objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md) amends [ADR 0026](../objects/architecture-decision/0026-use-one-kernel-with-namespaced-orlix-machines.md) and owns the updated lifecycle model.
