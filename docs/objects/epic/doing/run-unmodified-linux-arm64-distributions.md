---
type: epic
tags:
  - epic
  - linux-arm64
  - distributions
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#231"
summary: "Run unmodified Linux ARM64 distributions as OrlixInstances."
owned_by:
  - "[Orlix](../../product/orlix.md)"
has_story:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
---

# Run unmodified Linux ARM64 distributions

Run ordinary unmodified ARM64 Linux userspace inside OrlixInstances without binary rewriting, libc replacement, Apple-native linkage, Orlix ELF metadata, or custom Linux-visible syscall behavior. Completion requires the nine owned tasks, one unmodified musl distribution, one unmodified glibc distribution, native package workloads, and a broader distro matrix. This work does not weaken TCTI correctness or the ADR 0017 proof order.
