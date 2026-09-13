---
type: task
tags: [task, alpine, musl]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#237"
summary: "Boot unmodified Alpine ARM64 with its normal musl userspace and package manager."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
depends_on:
  - "[Check Linux ARM64 ABI equivalence](check-linux-arm64-abi-equivalence.md)"
  - "[Generalize distro-neutral root content](generalize-distro-neutral-root-content.md)"
blocks:
  - "[Prove native Linux package managers](prove-native-linux-package-managers.md)"
  - "[Prove diverse ARM64 distro compatibility](prove-diverse-arm64-distro-compatibility.md)"
---

# Boot unmodified Alpine ARM64

Boot Alpine with `/lib/ld-musl-aarch64.so.1`, BusyBox, its init and service model, shell, filesystem, and `apk`. Execute through Linux `execve()` and OrlixTCTI without OrlixMLibC or binary rewriting. Preserve Linux `aarch64` identity and attribute failures to the owning layer.
