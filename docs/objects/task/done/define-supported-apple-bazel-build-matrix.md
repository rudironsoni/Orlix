---
type: task
tags:
  - task
  - bazel
  - apple
  - compatibility
updated: 2026-09-06
status: done
summary: "Define every supported Apple build row, stable Make entry point, and Bazel authority contract before routing builds."
task_of:
  - "[Route every Apple build through Bazel](../../story/doing/route-every-apple-build-through-bazel.md)"
depends_on:
  - "[Run the Bazel Xcode 26.6 feasibility experiment](../doing/run-bazel-xcode-26-6-feasibility-experiment.md)"
blocks:
  - "[Implement universal Apple Bazel build routing](../doing/implement-universal-apple-bazel-build-routing.md)"
  - "[Implement shared Bazel cache and buildset reuse](../doing/implement-shared-bazel-cache-and-buildset-reuse.md)"
---

# Define The Supported Apple Bazel Build Matrix

Record the canonical Apple matrix after the feasibility task passes. The matrix must list every supported iOS, iPadOS, and macOS deployment and runtime version, destination, Orlix profile, Apple compilation mode, component mode, signing mode, test surface, and required proof environment.

The matrix must include the iOS and iPadOS 15.0 application and public SDK floor, the iOS 15.5 CI runtime proof installed with pinned `xcodes`, the optional iOS and iPadOS 16.1 Live Activity extension, and the Apple-silicon macOS 13.3 minimum from the accepted platform decision. Later supported versions must have explicit rows. An available simulator or SDK does not create a support claim by itself.

Map every current Make build, test, archive, release, and diagnostic name to one stable Make-owned operation. Make delegates to fixed Bazel targets. Direct Bazel labels remain maintainer diagnostics, and Xcode local or cloud projects remain frontends with no independent source graph.

Acceptance requires an explicit supported, gated, or unsupported result for every matrix row. The matrix must preserve the iOS 15 reduced feature set and name the availability or capability gate for every later-system feature. It must also define which rows require source mode, promoted signed buildsets, clean reconstruction, device compilation, simulator runtime proof, or macOS runtime proof.

The canonical machine-readable matrix is `bazel/migration/apple-build-matrix.json`. iOS 15.5 CI runtime and promoted-buildset rows stay `gated` until the owning oracles pass. Make plus XcodeGen remain product-compile authority until the single cutover.
