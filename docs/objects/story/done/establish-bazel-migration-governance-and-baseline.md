---
type: story
tags:
  - story
  - bazel
  - build-system
updated: 2026-09-01
status: done
summary: "Create the durable migration decisions, exhaustive current-build inventory, task envelope, and measured baseline."
story_of:
  - "[Adopt Bazel product graph](../../epic/done/adopt-bazel-product-graph.md)"
has_task:
  - "[Record the Bazel migration plan and baseline](../../task/done/record-bazel-migration-plan-and-baseline.md)"
---

# Establish Bazel Migration Governance And Baseline

As an Orlix maintainer, I want the migration scope, stop conditions, ownership rules, current target map, proof map, workflow map, and build baseline recorded before product graph changes begin.

The story is complete when no current Make, Xcode, workflow, package, or proof operation is unclassified, the stale `make build` cleanup claim is corrected, the current Apple toolchain is recorded, and the feasibility work has one bounded task envelope.
