---
type: story
tags: [story, bazel, proof]
updated: 2026-09-09
status: doing
summary: "Bind every proof tier to exact subject, buildset, profile, destination, prerequisite, and toolchain identities."
story_of:
  - "[Adopt Bazel product graph](../../epic/doing/adopt-bazel-product-graph.md)"
has_task:
  - "[Implement the digest-bound Bazel proof graph](../../task/doing/implement-digest-bound-bazel-proof-graph.md)"
---

# Bind Proof To Bazel Artifact Identities

As an Orlix maintainer, I want every proof report to identify the exact tested artifact and prerequisites so stale or mismatched evidence cannot authorize promotion.
