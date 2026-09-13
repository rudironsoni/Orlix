---
type: task
tags: [task, performance, cache]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#248"
summary: "Share immutable data-only TCTI translation templates across Linux processes."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Persist data-only translations](persist-data-only-translations.md)"
blocks:
  - "[Make TCTI caches SMP-safe](make-tcti-caches-smp-safe.md)"
---

# Share immutable translation templates

Content-address immutable templates while keeping process-local mapping validation, invalidation, and branch prediction separate. One process cannot change another's correctness. Avoid a global hot lock and prove identical multi-process loader and libc behavior before claiming reuse.
