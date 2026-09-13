---
type: story
tags: [story, performance, orlix-tcti]
updated: 2026-09-13
status: doing
summary: "Measure and optimize TCTI memory, decode, state, caches, virtio, Apple capacity, and layout."
story_of:
  - "[Maximize ARM64 execution performance](../../epic/doing/maximize-arm64-execution-performance.md)"
has_task:
  - "[Establish runtime performance baselines](../../task/doing/establish-runtime-performance-baselines.md)"
  - "[Use the TCTI TLB hot path](../../task/doing/use-tcti-tlb-hot-path.md)"
  - "[Eliminate cached decode work](../../task/doing/eliminate-cached-decode-work.md)"
  - "[Add measured static helpers](../../task/doing/add-measured-static-helpers.md)"
  - "[Keep guest state hot](../../task/doing/keep-guest-state-hot.md)"
  - "[Persist data-only translations](../../task/doing/persist-data-only-translations.md)"
  - "[Share immutable translation templates](../../task/doing/share-immutable-translation-templates.md)"
  - "[Research zero-copy guest page aliases](../../task/doing/research-zero-copy-guest-page-aliases.md)"
  - "[Tune Apple host CPU capacity](../../task/doing/tune-apple-host-cpu-capacity.md)"
  - "[Scale virtio data paths](../../task/doing/scale-virtio-data-paths.md)"
  - "[Apply measured PGO and layout](../../task/doing/apply-measured-pgo-and-layout.md)"
  - "[Research data-only Metal acceleration](../../task/doing/research-data-only-metal-acceleration.md)"
---

# Optimize measured TCTI execution

Use reproducible measurement to order CPU-first performance work. Correctness remains the oracle. Cache misses and disabled optimizations can change speed but not behavior.
