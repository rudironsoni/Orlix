---
type: task
tags:
  - task
  - bazel
  - cache
  - artifacts
updated: 2026-09-10
status: doing
summary: "Reuse compatible Bazel results and signed guest/native component buildsets across Apple builds without sharing mutable worktree state."
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

Use Bazel action-result caching for speed with identities that include every effective input that can change the result. Include destination, profile, compilation mode, or SDK identity only when that dimension changes output. A cache miss, outage, deletion, or corruption must leave the source build correct. Promotion disables action-result reuse, ccache, persistent Kbuild output, and DerivedData, but may use digest-verified source downloads and verified tool installations.

Use GHCR OCI artifacts for private Apple-native components and Linux guest/distribution components, plus one signed compatible buildset. `artifacts.lock.json` selects the buildset and immutable component digests. Normal app-only, Xcode Cloud, TestFlight, and release-candidate builds consume that buildset in promoted mode through OrlixKit. Source mode remains required for component changes, promotion, nightly reconstruction, and toolchain changes. Immutable GitHub Releases hold the public `OrlixKit.xcframework` and official release evidence.

Acceptance requires cache-on and cache-off output equivalence for reproducible outputs, rejection of wrong toolchain, destination, profile, or dependency identities, no release authorization from a cache hit alone, and successful reuse of compatible private artifacts without cross-worktree mutation or duplicate component builds.

The shared promoted-artifact store is local-first and digest-addressed. A warm hit for all locked artifacts performs zero downloads. Locked buildsets and active consumers are pinned against garbage collection. OrlixDistribution artifacts remain guest resources and are not hidden as Apple-native link dependencies.

BuildBuddy Cloud is the shared Bazel AC/CAS at `grpcs://remote.buildbuddy.io`. The compatible cache instance is `orlix/apple/bazel-9.2.0/xcode-17F113/v1`; `v1` is the tracked `ORLIX_BAZEL_CACHE_EPOCH`. The instance excludes commit, branch, pull-request, and simulator-runtime identity. Remote execution stays disabled. Cache-enabled builds use compression, content-defined chunking, minimal downloads by default, the local disk cache, and BuildBuddy BES. The canonical simulator-product operation requests top-level outputs only when it needs the app locally.

`ORLIX_BUILDBUDDY_CACHE_MODE` accepts only `normal`, `conserve`, or `off`, and defaults to `normal`. In `normal`, main reads and writes, same-repository pull requests read with a read-only key and disabled uploads, and forks stay off. In `conserve`, only main reads and writes. In `off`, all contexts stay off. Promotion, nightly independent reconstruction, TestFlight, and release do not use BuildBuddy action results. Local use is explicit through the untracked `.bazelrc.local`.

The operating ceiling is 80 GB of the 100 GB monthly transfer allowance. From 0 to less than 60 GB, keep `normal`. From 60 to less than 75 GB, keep `normal` and inspect actions with high transfer. From 75 to less than 80 GB, use `conserve`. At 80 GB or more, use `off` until the billing period resets. The 20 GB reserve is not an operating target. Optimize for CI time avoided per GB transferred, not cache-hit percentage alone. Publication artifacts stay in GHCR or release storage, never BuildBuddy.

The cache-equivalence gate uses independent seed, cached, and uncached output
bases. It requires observed disk-cache hits for UAPI, MLibC, and rootfs, then
local execution with action, disk, remote, compiler, and persistent Kbuild
reuse disabled. Full component-tree comparison covers paths, modes, symlinks,
and file contents. Digest marker equality alone cannot satisfy this gate.
The gate produces verification evidence without publishing artifacts or
changing the signed buildset lock.

Rootfs output representation must retain empty directories across cache
restoration. Bazel's [tree-artifact limitation](https://github.com/bazelbuild/bazel/issues/15901)
cannot justify removing those directories from the full-tree comparison.
Rootfs uses its existing filesystem images as the cacheable product outputs.
The producing action owns temporary assembly trees and validates their
contents in the images. The provider and app consumers share this image boundary.
