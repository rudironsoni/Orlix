---
type: task
tags: [task, linux-arm64, abi]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#234"
summary: "Machine-check arch/orlix userspace ABI equivalence against upstream Linux ARM64."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
depends_on:
  - "[Define Linux ARM64 ABI](define-linux-arm64-abi.md)"
blocks:
  - "[Prove mainstream distro kernel interfaces](prove-mainstream-distro-kernel-interfaces.md)"
  - "[Boot unmodified Alpine ARM64](boot-unmodified-alpine-arm64.md)"
  - "[Boot unmodified Debian or Ubuntu ARM64](boot-unmodified-debian-or-ubuntu-arm64.md)"
---

# Check Linux ARM64 ABI equivalence

Compare Orlix architecture identity, auxv, userspace register and signal layout, ptrace and core regsets, audit and seccomp constants, TLS, mmap, and selected UAPI against upstream ARM64. Run through Make, report exact mismatches and owners, keep generated upstream trees read-only, and prove the same result for release and development profiles.
