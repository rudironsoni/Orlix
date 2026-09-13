---
type: task
tags: [task, graphics, vulkan]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#262"
summary: "Add Vulkan acceleration through Venus on the established virtio-gpu path."
task_of:
  - "[Deliver Linux graphics through virtio-gpu](../../story/doing/deliver-linux-graphics-through-virtio-gpu.md)"
depends_on:
  - "[Implement Linux virtio-gpu and Metal backend](implement-linux-virtio-gpu-and-metal-backend.md)"
---

# Add Vulkan through Venus

Expose ordinary Linux Vulkan loader and ICD interfaces. Carry guest work through Venus and the existing virtio-gpu device. Keep Apple translation private. Prove synchronization, resource lifetime, presentation, resize, suspension, resume, error propagation, and representative Vulkan workloads. Preserve the OpenGL and Wayland path and do not create a second guest graphics contract.
