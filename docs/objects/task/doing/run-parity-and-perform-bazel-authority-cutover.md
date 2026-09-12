---
type: task
tags: [task, bazel, migration]
updated: 2026-09-09
status: doing
summary: "Run shadow parity, performance, reproducibility, and tamper tests, then perform the single build-authority cutover."
task_of:
  - "[Prove parity and cut over build authority](../../story/doing/prove-parity-and-cut-over-build-authority.md)"
depends_on:
  - "[Prove all supported Apple builds and feature gates](prove-all-supported-apple-builds-and-feature-gates.md)"
---

# Run Parity And Perform The Bazel Authority Cutover

Compare symbols, installed UAPI, sysroots, package trees, rootfs, images, framework layout, entitlements, resources, and proof output. After every gate passes, switch Make delegates, required checks, Xcode ownership, release workflows, and artifact selection, then remove the legacy build authorities.
