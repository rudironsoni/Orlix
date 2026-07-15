---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-15
summary: "Linux console selection and terminal application presentation are separate responsibilities."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Console and terminal

Linux console selection and terminal application presentation are separate responsibilities. The kernel exposes Linux consoles, while the app renders terminal UI and Herdr owns topology.
