---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-15
summary: "Durable port inputs live in source overlays, configurations, and patches."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Durable and generated trees

Durable port inputs live in source overlays, configurations, and patches. Generated upstream trees and build products are read-only evidence and must never be edited to force a pass.
