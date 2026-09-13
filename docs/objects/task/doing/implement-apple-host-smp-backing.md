---
type: task
tags: [task, apple, smp]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#252"
summary: "Provide private Apple execution capacity for each online Linux CPU."
task_of:
  - "[Execute real Linux SMP](../../story/doing/execute-real-linux-smp.md)"
depends_on:
  - "[Implement arch/orlix SMP foundations](implement-arch-orlix-smp-foundations.md)"
blocks:
  - "[Implement Linux IPI timer and idle](implement-linux-ipi-timer-and-idle.md)"
---

# Implement Apple host SMP backing

Give each online Linux CPU stable private host execution capacity. Linux remains the only scheduler. Idle does not busy-poll. Startup and shutdown release host resources. The private interface exposes neither guest instruction policy nor Linux scheduling policy.
