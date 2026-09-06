---
type: story
tags: [story, bazel, upstream]
updated: 2026-09-06
status: done
summary: "Wrap verified upstream sources, Kbuild, mlibc, packages, and rootfs assembly behind narrow Bazel contracts."
story_of:
  - "[Adopt Bazel product graph](../../epic/done/adopt-bazel-product-graph.md)"
has_task:
  - "[Implement verified upstream and foreign-build rules](../../task/done/implement-verified-upstream-and-foreign-build-rules.md)"
---

# Build Hermetic Upstream Component Boundaries

As an Orlix maintainer, I want Bazel to declare cross-component inputs and outputs while Kbuild, Meson, Ninja, Autotools, and upstream Make retain internal build ownership.
