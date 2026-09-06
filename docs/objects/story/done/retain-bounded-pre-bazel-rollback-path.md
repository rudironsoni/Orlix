---
type: story
tags: [story, bazel, rollback]
updated: 2026-09-06
status: done
summary: "Keep one read-only pre-Bazel rollback path until one release and one reconstruction prove the new authority."
story_of:
  - "[Adopt Bazel product graph](../../epic/done/adopt-bazel-product-graph.md)"
has_task:
  - "[Retire the pre-Bazel rollback branch](../../task/done/retire-pre-bazel-rollback-branch.md)"
---

# Retain A Bounded Pre-Bazel Rollback Path

As an Orlix release maintainer, I want a read-only pre-Bazel recovery path for one release cycle without allowing parallel feature development or mutable prior release evidence.
