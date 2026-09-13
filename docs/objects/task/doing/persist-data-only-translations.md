---
type: task
tags: [task, performance, cache]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#247"
summary: "Persist content-addressed data-only TCTI translations."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
depends_on:
  - "[Eliminate cached decode work](eliminate-cached-decode-work.md)"
blocks:
  - "[Share immutable translation templates](share-immutable-translation-templates.md)"
---

# Persist data-only translations

Persist semantic operation IDs and operands, never native code or function pointers. Identity covers guest code, TCTI schema, ISA and capability profile, and all lowering inputs. Corruption, stale identity, or version mismatch causes safe retranslation. Runtime mapping generation remains authoritative. Absence changes only performance.
