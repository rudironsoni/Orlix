---
type: task
tags:
  - task
  - bazel
  - cache
  - artifacts
updated: 2026-09-09
status: doing
summary: "Reuse compatible Bazel results and signed component buildsets across Apple builds without sharing mutable worktree state."
task_of:
  - "[Route every Apple build through Bazel](../../story/doing/route-every-apple-build-through-bazel.md)"
depends_on:
  - "[Define the supported Apple Bazel build matrix](../done/define-supported-apple-bazel-build-matrix.md)"
  - "[Implement component and buildset promotion](implement-component-and-buildset-promotion.md)"
  - "[Implement protected Bazel automation](implement-protected-bazel-automation.md)"
blocks:
  - "[Prove all supported Apple builds and feature gates](prove-all-supported-apple-builds-and-feature-gates.md)"
---

# Implement Shared Bazel Cache And Buildset Reuse

Apply the accepted storage split to every supported Apple build. Share only content-addressed or dependency-checked data across parallel worktrees: the bounded Bazel disk cache, repository download cache, and compiler-object cache. Keep each worktree's `ORLIX_BUILD_ROOT`, Bazel output base and server, execution root, Kbuild output, DerivedData, generated Xcode project, and proof outputs isolated.

Use Bazel action-result caching for speed with namespaces that include the exact Bazel version, Xcode build, SDK identity, destination, profile, compilation mode, and relevant target inputs. A cache miss, outage, deletion, or corruption must leave the source build correct. Promotion disables action-result reuse, ccache, persistent Kbuild output, and DerivedData, but may use digest-verified source downloads and verified tool installations.

Use GHCR OCI artifacts for private components and one signed compatible buildset. `artifacts.lock.json` selects the buildset and immutable component digests. Normal app-only, Xcode Cloud, TestFlight, and release-candidate builds consume that buildset in promoted mode. Source mode remains required for component changes, promotion, nightly reconstruction, and toolchain changes. Immutable GitHub Releases hold the public `OrlixOS.xcframework` and official release evidence.

Acceptance requires cache-on and cache-off output equivalence for reproducible outputs, rejection of wrong toolchain, destination, profile, or dependency identities, no release authorization from a cache hit alone, and successful reuse of compatible private artifacts without cross-worktree mutation or duplicate component builds.
