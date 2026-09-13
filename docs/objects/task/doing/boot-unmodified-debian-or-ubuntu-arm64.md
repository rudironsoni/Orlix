---
type: task
tags: [task, debian, glibc]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#238"
summary: "Boot unmodified Debian or Ubuntu ARM64 with normal glibc userspace."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
depends_on:
  - "[Check Linux ARM64 ABI equivalence](check-linux-arm64-abi-equivalence.md)"
  - "[Generalize distro-neutral root content](generalize-distro-neutral-root-content.md)"
  - "[Prove mainstream distro kernel interfaces](prove-mainstream-distro-kernel-interfaces.md)"
blocks:
  - "[Prove native Linux package managers](prove-native-linux-package-managers.md)"
  - "[Prove diverse ARM64 distro compatibility](prove-diverse-arm64-distro-compatibility.md)"
---

# Boot unmodified Debian or Ubuntu ARM64

Boot the normal glibc loader, system libraries, shell, multi-process init and service stack, and `apt` or `dpkg`. Do not replace the loader, rebuild against OrlixMLibC, rewrite guest binaries, or emulate services outside Linux. Preserve the ABI equivalence proof throughout.
