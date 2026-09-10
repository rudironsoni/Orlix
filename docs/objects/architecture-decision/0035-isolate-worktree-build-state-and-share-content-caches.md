---
type: architecture-decision
tags: [architecture, decision, bazel, worktrees, cache]
updated: 2026-09-10
status: accepted
external_id: "ADR-0035"
summary: "Give every worktree isolated mutable build state and share only bounded content-addressed or dependency-checked caches."
part_of:
  - "[Orlix](../product/orlix.md)"
amended_by:
  - "[ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md)"
---

# ADR 0035: Isolate Worktree Build State And Share Content Caches

## Status

Accepted.

## Context

Orlix developers use multiple parallel worktrees. Sharing mutable output trees can cross-contaminate source identities, generated files, Bazel servers, Kbuild objects, DerivedData, and proof results. Repeating every immutable download and identical action wastes storage and build time.

## Decision

Each worktree owns its literal `ORLIX_BUILD_ROOT`, Bazel output base and server, execution root, Kbuild `O=` tree, DerivedData, generated Xcode project, and literal `CCACHE_BASEDIR`.

Worktrees may share these bounded caches:

```text
~/Library/Caches/Orlix/Bazel/disk-cache       30 GiB, 30 days, namespaced by Bazel and Xcode build
~/Library/Caches/Orlix/Bazel/repository-cache 10 GiB, 90 days
~/Library/Caches/Orlix/ccache                  20 GiB, 30 days
```

The Bazel disk cache stores action results and content-addressed blobs. Every disk-cache namespace includes the exact Bazel version and Xcode build because Apple input discovery can otherwise reuse paths from a different selected Xcode. The repository cache stores only digest-verified downloads. ccache keys must include normalized source identity, compiler identity, and flags required by output semantics. `CCACHE_BASEDIR` remains a literal worktree-local setting for path normalization; it MUST NOT become an unnecessary cross-worktree cache namespace when normalized source paths and compiler inputs are equivalent.

The shared promoted-artifact CAS stores immutable OCI manifests, blobs, signatures, provenance, and verification records by digest. Its GC policy pins the locked buildset and active consumers and evicts only unused, unpinned content within the bounded budget.

The shared prepared-source and promoted-artifact store has a combined 30 GiB budget and 30-day eviction for unused, unpinned content. If protected content prevents acquisition within that budget, acquisition MUST fail with the required space rather than evict an active or locked artifact.

Canonical promotion disables action-result reuse, ccache, persistent Kbuild output, and DerivedData. It may use digest-verified repository downloads and exactly verified tool installations.

## Consequences

Parallel worktrees can build concurrently without shared mutable state. Identical safe content can be reused locally without copying full build trees.

A cache miss, deletion, outage, or corruption can affect speed only. It cannot change correctness or proof. Mutable incremental state remains worktree-local, and every action miss revalidates declared inputs before invoking its owning upstream engine.
