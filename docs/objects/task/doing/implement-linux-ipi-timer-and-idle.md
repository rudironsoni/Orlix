---
type: task
tags: [task, linux, smp]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#253"
summary: "Implement Linux IPI, per-CPU timer, wakeable idle, and TCTI safe points."
task_of:
  - "[Execute real Linux SMP](../../story/doing/execute-real-linux-smp.md)"
depends_on:
  - "[Implement arch/orlix SMP foundations](implement-arch-orlix-smp-foundations.md)"
  - "[Implement Apple host SMP backing](implement-apple-host-smp-backing.md)"
blocks:
  - "[Make TCTI caches SMP-safe](make-tcti-caches-smp-safe.md)"
  - "[Prove SMP memory ordering and atomics](prove-smp-memory-ordering-and-atomics.md)"
  - "[Tune Apple host CPU capacity](tune-apple-host-cpu-capacity.md)"
  - "[Scale virtio data paths](scale-virtio-data-paths.md)"
---

# Implement Linux IPI timer and idle

Use Linux-owned pending state and handlers for reschedule, call-function, stop, and required IPIs. Host mechanics only wake CPUs. Provide per-CPU one-shot and tickless clockevents, interruptible idle, and bounded TCTI Linux-control safe points without per-instruction polling. Prove scheduler, timer, IPI, and stress behavior.
