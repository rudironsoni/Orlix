---
type: task
tags: [task, distributions, compatibility]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#240"
summary: "Prove compatibility across diverse unmodified ARM64 distributions."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
depends_on:
  - "[Prove mainstream distro kernel interfaces](prove-mainstream-distro-kernel-interfaces.md)"
  - "[Boot unmodified Alpine ARM64](boot-unmodified-alpine-arm64.md)"
  - "[Boot unmodified Debian or Ubuntu ARM64](boot-unmodified-debian-or-ubuntu-arm64.md)"
  - "[Prove native Linux package managers](prove-native-linux-package-managers.md)"
---

# Prove diverse ARM64 distro compatibility

Run Fedora, Arch Linux ARM, and NixOS, or documented equivalents, without rewriting ELF or replacing libc and loaders. Reach useful multi-process userspace and one native package or system operation per distro. Record release and root identity, libc, init, package stack, failures, skips, and evidence. Fix shared owners, not distro-specific app shims.
