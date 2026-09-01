---
type: story
tags:
  - story
  - bazel
  - apple
  - cache
  - artifacts
updated: 2026-09-01
status: todo
summary: "Route every supported iOS, iPadOS, and macOS build through Bazel while reusing only compatible verified outputs."
story_of:
  - "[Adopt Bazel product graph](../../epic/doing/adopt-bazel-product-graph.md)"
has_task:
  - "[Define the supported Apple Bazel build matrix](../../task/todo/define-supported-apple-bazel-build-matrix.md)"
  - "[Implement universal Apple Bazel build routing](../../task/todo/implement-universal-apple-bazel-build-routing.md)"
  - "[Implement shared Bazel cache and buildset reuse](../../task/todo/implement-shared-bazel-cache-and-buildset-reuse.md)"
  - "[Prove all supported Apple builds and feature gates](../../task/todo/prove-all-supported-apple-builds-and-feature-gates.md)"
depends_on:
  - "[Prove the Bazel Apple feasibility gate](../doing/prove-bazel-apple-feasibility-gate.md)"
blocks:
  - "[Prove parity and cut over build authority](prove-parity-and-cut-over-build-authority.md)"
---

# Route Every Apple Build Through Bazel

As an Orlix maintainer, I want every supported iOS, iPadOS, and macOS build to use the one Bazel product graph and safe reusable build inputs so Apple builds do not repeat compatible component work or consume stale state.

This story starts only after the Bazel Apple feasibility gate passes. It covers every supported deployment and runtime version in the declared Apple matrix, including the iOS and iPadOS 15 reduced-feature product, the optional iOS and iPadOS 16.1 Live Activity extension, and the Apple-silicon macOS product with its accepted minimum and later supported versions.

Make remains the only supported repository-owned command interface. Local generated Xcode projects and the committed Xcode Cloud discovery project are frontends. After the final authority cutover, they must delegate to Make and Bazel and must not maintain an independent product graph.

Source mode remains the authority for component changes, promotion, reconstruction, and toolchain changes. Promoted mode consumes one signed buildset selected by `artifacts.lock.json`. Shared caches may accelerate builds only when their inputs are content-addressed or dependency-checked, and each worktree keeps isolated mutable output state.

The iOS 15 product keeps its reduced capability contract. Newer APIs, Live Activities, MLX execution, and other later-system features remain explicitly gated and must not load or execute on iOS 15. A feature that is not supported on a matrix row remains unavailable on that row instead of being silently replaced by an unsafe implementation.

The story is complete when every matrix row and build surface routes through Bazel, compatible action and promoted-artifact reuse is proven, all required iOS 15 and newer-system gates pass in their owning environments, macOS rows are covered, and the exact evidence is bound to the tested artifact or buildset digest before authority cutover.
