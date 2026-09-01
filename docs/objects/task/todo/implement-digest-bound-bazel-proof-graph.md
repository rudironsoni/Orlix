---
type: task
tags: [task, bazel, proof]
updated: 2026-09-01
status: todo
summary: "Map all current proof targets to Bazel and bind reports to exact artifact and buildset digests."
task_of:
  - "[Bind proof to Bazel artifact identities](../../story/todo/bind-proof-to-bazel-artifact-identities.md)"
---

# Implement The Digest-Bound Bazel Proof Graph

Preserve ADR 0017 ordering and current test ownership. Use analysis tests only for graph properties, execution tests for generated contents, workflow tests for automation policy, and fault injection for tampering and environment failures.
