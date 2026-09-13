---
type: task
tags: [task, virtio, performance]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#257"
summary: "Scale virtio storage, network, and filesystem paths for Linux SMP."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Establish runtime performance baselines](establish-runtime-performance-baselines.md)"
  - "[Implement Linux IPI timer and idle](implement-linux-ipi-timer-and-idle.md)"
---

# Scale virtio data paths

Measure and use upstream multiqueue, notification suppression, batching, scatter-gather, EVENT_IDX, and valid network offloads. Scale virtio-blk, virtio-net, and the selected filesystem transport without changing ordering or Linux-visible semantics. HostAdapter owns backend mechanics only. Record queue depth, crossings, throughput, latency, CPU, and failures.
