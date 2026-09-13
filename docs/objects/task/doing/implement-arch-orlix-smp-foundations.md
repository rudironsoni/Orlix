---
type: task
tags: [task, linux, smp]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#251"
summary: "Implement arch/orlix percpu state and upstream Linux SMP bring-up."
task_of:
  - "[Execute real Linux SMP](../../story/doing/execute-real-linux-smp.md)"
depends_on:
  - "[Establish runtime performance baselines](establish-runtime-performance-baselines.md)"
blocks:
  - "[Implement Apple host SMP backing](implement-apple-host-smp-backing.md)"
  - "[Implement Linux IPI timer and idle](implement-linux-ipi-timer-and-idle.md)"
---

# Implement arch/orlix SMP foundations

Enable `CONFIG_SMP` with upstream percpu allocation, CPU masks and bring-up, CPU-local `current`, thread state, and IRQ state. Use Linux CPU possible, present, and online lifecycle. Prove multiple app-hosted CPUs and catch cross-CPU state corruption with KUnit and kselftest.
