---
type: task
tags: [task, bazel, upstream]
updated: 2026-09-06
status: done
summary: "Implement pinned source preparation, narrow providers, Kbuild, UAPI, mlibc, package, rootfs, initramfs, and ext4 rules."
task_of:
  - "[Build hermetic upstream component boundaries](../../story/done/build-hermetic-upstream-component-boundaries.md)"
---

# Implement Verified Upstream And Foreign Build Rules

Implement source mirrors, hash verification, deterministic patch and overlay application, narrow typed providers, canonical clean actions, and developer-only persistent Kbuild acceleration. Do not replace upstream internal dependency graphs or edit generated trees.
