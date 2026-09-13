---
type: task
tags: [task, linux, atomics]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#255"
summary: "Implement exact Linux memory ordering and guest atomic semantics under SMP."
task_of:
  - "[Execute real Linux SMP](../../story/doing/execute-real-linux-smp.md)"
depends_on:
  - "[Implement Linux IPI timer and idle](implement-linux-ipi-timer-and-idle.md)"
---

# Prove SMP memory ordering and atomics

Map relaxed, acquire, release, acquire-release, and sequential ordering exactly. Preserve CAS, SWP, LDADD, exclusive-monitor, width, alignment, fault, success, failure, and ordering semantics. Use fixed host atomics only when equivalent. Run locking, atomic, litmus, and kselftest proof with multiple CPUs and retain equivalent single-CPU behavior.
