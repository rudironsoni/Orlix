---
type: task
tags: [task, bazel, apple]
updated: 2026-09-01
status: todo
summary: "Implement explicit Bazel targets for HostAdapter, hosted Kernel composition, native layers, OrlixOS, Orlix, extensions, and tests."
task_of:
  - "[Move the Apple product graph to Bazel](../../story/todo/move-apple-product-graph-to-bazel.md)"
---

# Implement The Bazel Apple Product Graph

Determine the HostAdapter composition edge from symbol evidence. Preserve target-specific Swift language modes, all native libraries and frameworks, resources, privacy data, StoreKit configuration, entitlements, deployment targets, and signing behavior. Generate local Xcode projects and commit only the narrow Xcode Cloud bootstrap project.
