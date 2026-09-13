---
type: task
tags: [task, performance, static-helpers]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#245"
summary: "Add measured fixed native AArch64 helpers and data-only superinstructions."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Eliminate cached decode work](eliminate-cached-decode-work.md)"
blocks:
  - "[Apply measured PGO and layout](apply-measured-pgo-and-layout.md)"
---

# Add measured static helpers

Select candidates from baseline profiles. Dispatch only to fixed precompiled signed helpers whose exact register, flag, memory, fault, signal, ordering, and PC behavior matches the proved reference path. Superinstructions remain data. Each specialization has differential and KUnit proof and can be disabled without changing semantics.
