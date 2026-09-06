---
type: story
tags: [story, bazel, proof]
updated: 2026-09-06
status: done
summary: "Bind every proof tier to exact subject, buildset, profile, destination, prerequisite, and toolchain identities."
story_of:
  - "[Adopt Bazel product graph](../../epic/done/adopt-bazel-product-graph.md)"
has_task:
  - "[Implement the digest-bound Bazel proof graph](../../task/done/implement-digest-bound-bazel-proof-graph.md)"
---

# Bind Proof To Bazel Artifact Identities

As an Orlix maintainer, I want every proof report to identify the exact tested artifact and prerequisites so stale or mismatched evidence cannot authorize promotion.
