---
type: task
tags: [task, orlix-tcti, smp]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#254"
summary: "Make TCTI translation caches safe and scalable across Linux CPUs."
task_of:
  - "[Execute real Linux SMP](../../story/doing/execute-real-linux-smp.md)"
depends_on:
  - "[Share immutable translation templates](share-immutable-translation-templates.md)"
  - "[Implement Linux IPI timer and idle](implement-linux-ipi-timer-and-idle.md)"
blocks:
  - "[Apply measured PGO and layout](apply-measured-pgo-and-layout.md)"
---

# Make TCTI caches SMP-safe

Use small CPU-local hot caches, immutable shared blocks, CPU-local successor state, and correct shared insertion and invalidation. A CPU cannot use a translation after guest mapping invalidation. Prove concurrent results against single-CPU execution and measure contention and scaling.
