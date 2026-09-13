---
type: task
tags: [task, graphics, virtio-gpu]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#261"
summary: "Implement upstream Linux virtio-gpu with a private Metal host backend."
task_of:
  - "[Deliver Linux graphics through virtio-gpu](../../story/doing/deliver-linux-graphics-through-virtio-gpu.md)"
targets:
  - "[OrlixKernel](../../software-component/orlixkernel.md)"
  - "[OrlixHostAdapter](../../software-component/orlixhostadapter.md)"
  - "[OrlixOS](../../software-component/orlixos.md)"
blocks:
  - "[Add Vulkan through Venus](add-vulkan-through-venus.md)"
---

# Implement Linux virtio-gpu and Metal backend

Use upstream Linux virtio-gpu and DRM interfaces. Run Mesa and display userspace as ordinary Linux components without Apple linkage. Keep rendering and presentation private to the host backend. Prove buffer lifetime, synchronization, resize, suspension, resume, and presentation. Demonstrate an accelerated Wayland or Weston path and retain an unaccelerated reference path for diagnosis.
