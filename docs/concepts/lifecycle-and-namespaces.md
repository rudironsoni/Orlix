---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-26
summary: "Each OrlixMachine is a persistent namespaced Linux userspace system hosted by one upstream Linux kernel."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Lifecycle and namespaces

One upstream Linux kernel may host multiple persistent `OrlixMachine` systems using Linux namespaces and cgroups. Each machine owns its userspace lifecycle, root, and namespace identity without pretending to be a separate kernel or VM. Apple application lifecycle maps onto Linux suspend and hibernation without replacing Linux task semantics.

`OrlixMachine` replaces the rejected Local Runtime and Local Instance public vocabulary without compatibility aliases. [ADR 0026](../objects/architecture-decision/0026-use-one-kernel-with-namespaced-orlix-machines.md) owns the lifecycle model.
