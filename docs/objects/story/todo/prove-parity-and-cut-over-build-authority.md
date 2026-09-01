---
type: story
tags: [story, bazel, migration]
updated: 2026-09-01
status: todo
summary: "Prove old and Bazel graph parity, then switch Make, Xcode, CI, release, and artifact authority once."
story_of:
  - "[Adopt Bazel product graph](../../epic/doing/adopt-bazel-product-graph.md)"
has_task:
  - "[Run parity and perform the Bazel authority cutover](../../task/todo/run-parity-and-perform-bazel-authority-cutover.md)"
depends_on:
  - "[Route every Apple build through Bazel](route-every-apple-build-through-bazel.md)"
---

# Prove Parity And Cut Over Build Authority

As an Orlix maintainer, I want measured semantic parity and failure injection before one final authority switch removes competing XcodeGen, Make orchestration, timestamp, and vendor build paths.
