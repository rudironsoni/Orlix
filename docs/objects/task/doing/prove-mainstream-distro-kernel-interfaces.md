---
type: task
tags: [task, linux, distributions]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#236"
summary: "Enable and prove upstream Linux interfaces required by mainstream distributions."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
depends_on:
  - "[Check Linux ARM64 ABI equivalence](check-linux-arm64-abi-equivalence.md)"
blocks:
  - "[Boot unmodified Debian or Ubuntu ARM64](boot-unmodified-debian-or-ubuntu-arm64.md)"
  - "[Prove diverse ARM64 distro compatibility](prove-diverse-arm64-distro-compatibility.md)"
---

# Prove mainstream distro kernel interfaces

Use upstream Linux implementations and pristine kselftests for seccomp, `clone3`, rseq, futex interfaces, pidfd, eventfd, timerfd, signalfd, epoll, namespaces, cgroup v2, and discovered distro blockers. Reduce failures to their Linux, libc, or TCTI owner. Do not emulate Linux policy in OrlixHostAdapter.
