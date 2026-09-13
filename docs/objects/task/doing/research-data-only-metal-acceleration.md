---
type: task
tags: [task, research, metal]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#259"
summary: "Research fixed Metal compute for bounded batches of data-only TCTI operations."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Establish runtime performance baselines](establish-runtime-performance-baselines.md)"
---

# Research data-only Metal acceleration

Benchmark fixed precompiled Metal compute for bounded, independent, register-only or similarly safe operation batches. Compare exact CPU and Metal results. Measure dispatch, crossover size, throughput, latency, energy, thermal wins, and losses. Never include syscalls, faults, signals, guest-memory side effects, generated source, JIT, RWX, or host-executable guest text. Product adoption requires a new ADR. This research blocks no product work.
