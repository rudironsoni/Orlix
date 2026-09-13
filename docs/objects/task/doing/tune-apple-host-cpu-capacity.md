---
type: task
tags: [task, apple, performance]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#256"
summary: "Tune Linux CPU capacity with supported Apple host signals."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Establish runtime performance baselines](establish-runtime-performance-baselines.md)"
  - "[Implement Linux IPI timer and idle](implement-linux-ipi-timer-and-idle.md)"
---

# Tune Apple host CPU capacity

Use supported processor-count, thermal, low-power, memory, and QoS signals without private affinity APIs, guest-visible Apple topology, or scheduler bypass. Change Linux CPU capacity through Linux CPU lifecycle. Measure sustained throughput, responsiveness, and thermal behavior. Conservative absence behavior preserves ABI and HWCAP.
