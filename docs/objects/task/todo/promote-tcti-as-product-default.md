---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-15
status: todo
summary: "Promote TCTI only after the real app-hosted Linux product path satisfies every gate."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
---

# Promote TCTI as the product default

Flip the product default only when real `/init` reaches `svc #0`, enters Linux syscall dispatch, emits Linux console output through the app-hosted path, and every forbidden executable-memory behavior check is false.
