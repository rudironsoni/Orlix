---
type: story
tags: [story, bazel, apple]
updated: 2026-09-09
status: doing
summary: "Prove the stable Bazel Apple matrix with Xcode 26.6 and every critical Orlix dependency before broad migration."
story_of:
  - "[Adopt Bazel product graph](../../epic/doing/adopt-bazel-product-graph.md)"
has_task:
  - "[Run the Bazel Xcode 26.6 feasibility experiment](../../task/doing/run-bazel-xcode-26-6-feasibility-experiment.md)"
blocks:
  - "[Route every Apple build through Bazel](route-every-apple-build-through-bazel.md)"
---

# Prove The Bazel Apple Feasibility Gate

As an Orlix maintainer, I want the hardest Bazel and Apple compatibility risks tested first so an unsupported Xcode, package, native dependency, signing, or cloud workflow stops the migration before broad rewrites.

The story is complete only when the complete feasibility gate in the migration concept passes without a permanent rules fork.
