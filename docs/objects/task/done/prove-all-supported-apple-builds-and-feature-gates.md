---
type: task
tags:
  - task
  - bazel
  - apple
  - proof
  - compatibility
updated: 2026-09-06
status: done
summary: "Prove every supported Apple matrix row uses Bazel, reuses only valid inputs, and keeps newer features gated on iOS 15."
task_of:
  - "[Route every Apple build through Bazel](../../story/done/route-every-apple-build-through-bazel.md)"
depends_on:
  - "[Implement universal Apple Bazel build routing](implement-universal-apple-bazel-build-routing.md)"
  - "[Implement shared Bazel cache and buildset reuse](implement-shared-bazel-cache-and-buildset-reuse.md)"
  - "[Implement the digest-bound Bazel proof graph](implement-digest-bound-bazel-proof-graph.md)"
blocks:
  - "[Run parity and perform the Bazel authority cutover](run-parity-and-perform-bazel-authority-cutover.md)"
---

# Prove All Supported Apple Builds And Feature Gates

Run the required proof for every supported iOS, iPadOS, and macOS matrix row through the Make-to-Bazel path. Prove source and promoted modes where the matrix permits them, device and simulator compilation, simulator runtime proof, macOS product proof, archives and signing inputs, test selection, and Xcode Cloud integration.

The iOS and iPadOS 15 row must install and boot the required runtime in protected CI with pinned `xcodes`. It must launch the app and prove the supported reduced feature set. Live Activities, MLX execution, and every other later-system API must remain unavailable on iOS 15 and must not load or execute. The iOS and iPadOS 16.1 Live Activity row proves the optional extension separately. Later iOS, iPadOS, and macOS rows require evidence only when the supported matrix declares them, but every declared row must have an explicit proof result.

Check the action graph, Make and Xcode frontends, target settings, dependency identities, cache policy, buildset digest, and proof subject. Bind each report to the exact artifact or buildset digest, profile, destination, toolchain, and prerequisite reports. Preserve ADR 0017 ordering. Do not treat compilation, simulator launch alone, a cache hit, or an unavailable local runtime as product runtime proof.

Acceptance requires no Apple build path that bypasses Bazel, no stale or incompatible artifact reuse, passing iOS 15 gated-feature checks, passing every declared newer-system row, clean and cache-enabled equivalence for reproducible outputs, and complete evidence ready for the later parity and authority-cutover task. This task does not itself authorize the authority cutover.
