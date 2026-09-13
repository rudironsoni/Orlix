---
type: task
tags: [task, research, memory]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#249"
summary: "Research zero-copy aliases for eligible non-executable Linux guest pages."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Use the TCTI TLB hot path](use-tcti-tlb-hot-path.md)"
---

# Research zero-copy guest page aliases

Limit the prototype to non-executable mappings. Preserve Linux VM ownership, permissions, COW, unmap, protection, fork, shared mapping, fault, replacement, and mapping-generation invalidation. Keep the reference path. Adoption requires complete correctness and measured benefit.
