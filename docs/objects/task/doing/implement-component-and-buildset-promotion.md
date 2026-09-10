---
type: task
tags: [task, bazel, artifacts]
updated: 2026-09-10
status: doing
summary: "Implement clean component promotion, signed buildsets, lock proposal pull requests, provenance, verification, and retention."
task_of:
  - "[Promote signed component buildsets](../../story/doing/promote-signed-component-buildsets.md)"
blocks:
  - "[Implement shared Bazel cache and buildset reuse](implement-shared-bazel-cache-and-buildset-reuse.md)"
---

# Implement Component And Buildset Promotion

Build each changed component twice without action-result reuse, compare reproducible unsigned outputs, run owning proof, sign and publish by digest, verify after pull, and propose compatible buildset lock changes through a protected pull request.

UAPI dual-build comparison now checks the entire output tree, including the Kbuild archive. Kbuild records relative source paths, and the archive fixes ownership and timestamps. This closes the archive reproducibility defect without changing upstream file contents. MLibC full-tree comparison still fails, rootfs comparison remains pending, and signed publication requires the existing trusted key.
