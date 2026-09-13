---
type: task
tags: [task, packages, app-store]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#239"
summary: "Prove native package managers and downloaded Linux guest software."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
depends_on:
  - "[Define guest executable policy](define-guest-executable-policy.md)"
  - "[Boot unmodified Alpine ARM64](boot-unmodified-alpine-arm64.md)"
  - "[Boot unmodified Debian or Ubuntu ARM64](boot-unmodified-debian-or-ubuntu-arm64.md)"
blocks:
  - "[Prove diverse ARM64 distro compatibility](prove-diverse-arm64-distro-compatibility.md)"
---

# Prove native Linux package managers

Run representative `apk add` and `apt install` or `dpkg` operations. Preserve downloaded ELF identity, loader, libraries, hashes, permissions, and Linux execution domain except for normal package effects. Exercise networking, TLS, filesystem, process, and signal behavior without app-side shims.
