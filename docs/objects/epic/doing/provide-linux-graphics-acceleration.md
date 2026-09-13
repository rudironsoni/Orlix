---
type: epic
tags: [epic, graphics, virtio-gpu]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#260"
summary: "Provide Linux graphics acceleration through virtio-gpu while Metal stays private."
owned_by:
  - "[Orlix](../../product/orlix.md)"
has_story:
  - "[Deliver Linux graphics through virtio-gpu](../../story/doing/deliver-linux-graphics-through-virtio-gpu.md)"
---

# Provide Linux graphics acceleration

Expose standard Linux DRM, virtio-gpu, Mesa, Wayland, and Vulkan interfaces to guest userspace. Metal remains a private Apple host backend and never becomes guest ABI. Prove rendering, presentation, synchronization, resize, suspension, resume, and lifecycle through the app-hosted product. Vulkan reuses the established virtio-gpu transport. TCTI Metal research is independent and non-gating.
