---
type: task
tags: [task, performance, state]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#246"
summary: "Keep eligible guest architectural state local across a TCTI block."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Establish runtime performance baselines](establish-runtime-performance-baselines.md)"
blocks:
  - "[Apply measured PGO and layout](apply-measured-pgo-and-layout.md)"
---

# Keep guest state hot

Reduce redundant `pt_regs` and state traffic inside translated blocks. Materialize PC, flags, registers, TLS-visible state, and other selected fields at every observable syscall, signal, fault, debug, proof, exit, interruption, invalidation, and completion boundary. Differential and KUnit tests cover normal and early exits.
