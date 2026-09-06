---
type: story
tags: [story, bazel, artifacts]
updated: 2026-09-06
status: done
summary: "Promote reproducible private components and compatible signed buildsets by immutable OCI digest."
story_of:
  - "[Adopt Bazel product graph](../../epic/done/adopt-bazel-product-graph.md)"
has_task:
  - "[Implement component and buildset promotion](../../task/done/implement-component-and-buildset-promotion.md)"
---

# Promote Signed Component Buildsets

As an Orlix integrator, I want normal app and release builds to reuse one verified set of private components without weakening source reconstruction or proof.
