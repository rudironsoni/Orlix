---
type: task
tags: [task, performance, memory]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#243"
summary: "Put the TCTI TLB on the production guest memory hot path."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Establish runtime performance baselines](establish-runtime-performance-baselines.md)"
blocks:
  - "[Research zero-copy guest page aliases](research-zero-copy-guest-page-aliases.md)"
---

# Use the TCTI TLB hot path

Use validated TLB mappings for eligible instruction fetches and scalar loads and stores. Validate page, access, Linux permissions, address space, and translation generation. Keep Linux MM as the miss, cross-page, complex-mapping, fault, COW, and invalidation path. Prove byte and fault equivalence against the slow path before widening writes or executable mappings.
