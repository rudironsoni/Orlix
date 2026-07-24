---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-24
status: todo
summary: "Promote TCTI only after every owning suite proves the real app-hosted Linux product path."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
depends_on:
  - "[Complete AArch64 ISA-on-ISA coverage](../doing/complete-aarch64-isa-on-isa-coverage.md)"
  - "[Complete the pinned simulator TCTI ladder](../doing/complete-pinned-simulator-tcti-ladder.md)"
blocks:
  - "[Validate the mobile terminal simulator product](../doing/validate-mobile-terminal-simulator-product.md)"
---

# Promote TCTI as the product default

Flip the product default only when all 4,350 leaves in the complete pinned Arm target are classified, every applicable EL0 semantic variant has complete production decode, lowering, semantics, and typed proof, and every non-EL0 classification has its required rejection proof. The narrower runtime HWCAP and HWCAP2 projection cannot make this target complete. Promotion also requires real `/init` to reach `svc #0`, enter Linux syscall dispatch, emit Linux console output through the app-hosted path, and leave every forbidden executable-memory behavior check false.
