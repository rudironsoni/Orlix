---
type: task
tags: [task, performance, decode]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#244"
summary: "Execute cached data-only TCTI operations without repeated AArch64 decode."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Establish runtime performance baselines](establish-runtime-performance-baselines.md)"
blocks:
  - "[Add measured static helpers](add-measured-static-helpers.md)"
  - "[Persist data-only translations](persist-data-only-translations.md)"
---

# Eliminate cached decode work

Decode on block creation, miss, or invalidation, not on a normal cache hit. Keep lowered blocks as data that selects precompiled signed handlers. Preserve unsupported encodings and structured exits. Use KUnit and differential proof for specialized hot classes, then measure decode-call and workload changes.
