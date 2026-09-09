---
type: epic
tags:
  - epic
  - bazel
  - build-system
updated: 2026-09-09
status: doing
summary: "Adopt Bazel as the Orlix repository product graph without weakening upstream ownership, proof, release identity, or worktree isolation."
targets:
  - "[Orlix](../../product/orlix.md)"
applies:
  - "[Bazel product graph migration](../../../concepts/bazel-product-graph-migration.md)"
has_story:
  - "[Establish Bazel migration governance and baseline](../../story/done/establish-bazel-migration-governance-and-baseline.md)"
  - "[Prove the Bazel Apple feasibility gate](../../story/doing/prove-bazel-apple-feasibility-gate.md)"
  - "[Route every Apple build through Bazel](../../story/doing/route-every-apple-build-through-bazel.md)"
  - "[Build hermetic upstream component boundaries](../../story/done/build-hermetic-upstream-component-boundaries.md)"
  - "[Move the Apple product graph to Bazel](../../story/doing/move-apple-product-graph-to-bazel.md)"
  - "[Bind proof to Bazel artifact identities](../../story/doing/bind-proof-to-bazel-artifact-identities.md)"
  - "[Promote signed component buildsets](../../story/doing/promote-signed-component-buildsets.md)"
  - "[Operate Bazel through protected automation](../../story/doing/operate-bazel-through-protected-automation.md)"
  - "[Prove parity and cut over build authority](../../story/doing/prove-parity-and-cut-over-build-authority.md)"
  - "[Retain a bounded pre-Bazel rollback path](../../story/doing/retain-bounded-pre-bazel-rollback-path.md)"
---

# Adopt Bazel Product Graph

Adopt Bazel as the repository product graph while Make remains the supported command interface and upstream build engines remain authoritative inside Linux, mlibc, and imported packages.

The epic is complete when the feasibility gate passes, source and promoted build modes are reproducible and fail closed, the complete proof ladder binds to exact buildset digests, worktrees share only safe caches, Xcode local and cloud workflows work, the final authority cutover removes competing build ownership, and one shipped release plus clean reconstruction completes the rollback window.

[CORRECTION] 2026-09-09: this epic is not done. `artifacts.lock.json` names `localhost:5001` for uapi, mlibc, and rootfs. That registry is not running. GHCR has no `orlix`, `orlix/uapi`, `orlix/mlibc`, or `orlix/rootfs` packages. Unsigned dual-build does not publish. Promoted mode still does not substitute OCI components. posix-shell TAP END is missing. Shadow parity, cutover, and release remain open.
