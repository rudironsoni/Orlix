---
type: task
tags: [task, performance, pgo]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#258"
summary: "Apply PGO and native layout only after structural hot-path work."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Add measured static helpers](add-measured-static-helpers.md)"
  - "[Keep guest state hot](keep-guest-state-hot.md)"
  - "[Make TCTI caches SMP-safe](make-tcti-caches-smp-safe.md)"
---

# Apply measured PGO and layout

Use versioned representative training workloads after structural optimization. Preserve reproducible outputs and Linux behavior. Measure against the same baseline identities and record cold start, code size, memory, and unrelated regressions. Release builds cannot depend on undeclared mutable local profiles.
