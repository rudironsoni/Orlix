---
type: story
tags: [story, linux, smp]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#250"
summary: "Execute real Linux SMP concurrently on Apple CPUs."
story_of:
  - "[Maximize ARM64 execution performance](../../epic/doing/maximize-arm64-execution-performance.md)"
depends_on:
  - "[Establish runtime performance baselines](../../task/doing/establish-runtime-performance-baselines.md)"
has_task:
  - "[Implement arch/orlix SMP foundations](../../task/doing/implement-arch-orlix-smp-foundations.md)"
  - "[Implement Apple host SMP backing](../../task/doing/implement-apple-host-smp-backing.md)"
  - "[Implement Linux IPI timer and idle](../../task/doing/implement-linux-ipi-timer-and-idle.md)"
  - "[Make TCTI caches SMP-safe](../../task/doing/make-tcti-caches-smp-safe.md)"
  - "[Prove SMP memory ordering and atomics](../../task/doing/prove-smp-memory-ordering-and-atomics.md)"
---

# Execute real Linux SMP

Bring more than one Linux CPU online with private Apple execution capacity. Linux alone schedules tasks and owns interrupts, timers, processes, signals, and ordering. Host mechanics provide capacity and wakeups. Prove CPU lifecycle, safe points, shared TCTI state, atomics, kselftests, and multi-process scaling without changing userspace ABI.
