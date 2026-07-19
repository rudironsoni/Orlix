---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-17
status: todo
summary: "Promote TCTI only after every owning suite proves the real app-hosted Linux product path."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
depends_on:
  - "[Complete AArch64 ISA-on-ISA coverage](../doing/complete-aarch64-isa-on-isa-coverage.md)"
  - "[Run authorized TCTI device validation](run-authorized-tcti-device-validation.md)"
blocks:
  - "[Validate the mobile terminal simulator product](../doing/validate-mobile-terminal-simulator-product.md)"
---

# Promote TCTI as the product default

Flip the product default only when the guest-exposed AArch64 EL0 ISA profile has complete decode, lowering, semantics, and exception coverage, real `/init` reaches `svc #0`, enters Linux syscall dispatch, emits Linux console output through the app-hosted path, and every forbidden executable-memory behavior check is false.
