---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-09-10
status: active
summary: "Apple-platform product application, terminal presentation, and user interaction."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# Orlix native app

Apple-platform product application, terminal presentation, and user interaction. Its product name and target name are `Orlix`; it consumes the public OrlixKit SDK and does not publish a second application or SDK identity. Its vvterm-derived terminal surface remains in the app. The app enters the local Linux runtime through OrlixKit and does not directly depend on private runtime implementation or guest distribution providers.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
