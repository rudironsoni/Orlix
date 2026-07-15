---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-15
summary: "One upstream Linux kernel may host isolated local instances using Linux namespaces and cgroups."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Lifecycle and namespaces

One upstream Linux kernel may host isolated local instances using Linux namespaces and cgroups. Apple application lifecycle maps onto Linux suspend and hibernation without replacing Linux task semantics.
