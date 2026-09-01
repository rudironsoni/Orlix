---
type: architecture-decision
tags: [architecture, decision, artifacts, provenance]
updated: 2026-09-01
status: accepted
external_id: "ADR-0034"
summary: "Use signed OCI component buildsets for normal integration and retain source mode for promotion and independent reconstruction."
part_of:
  - "[Orlix](../product/orlix.md)"
relates_to:
  - "[ADR 0017](0017-product-runtime-claim-promotion-order.md)"
  - "[ADR 0030](0030-use-one-public-orlixos-sdk-and-private-static-implementation-layers.md)"
---

# ADR 0034: Consume Signed Promoted Buildsets

## Status

Accepted.

## Context

Rebuilding Kernel, installed UAPI, mlibc, Coreutils, packages, rootfs, and private host components for every app-only change wastes Apple build time. Reusing independently selected mutable component tags can create combinations that were never tested together.

## Decision

Private component promotion uses two independent clean source builds without Bazel action-result cache, ccache, persistent Kbuild output, or DerivedData. Promotion compares reproducible unsigned outputs, runs owning proof, generates an SBOM and in-toto provenance, signs the OCI digest with the protected Orlix Cosign key, publishes by digest, and verifies a fresh pull.

A signed buildset records the compatible immutable digests for all private components, the toolchain manifest, and the proof index. `artifacts.lock.json` selects one buildset. Normal app-only integration, Xcode Cloud, TestFlight, and release-candidate builds consume that verified buildset in promoted mode.

Source mode remains required for component-changing pull requests, component promotion, nightly clean reconstruction, toolchain changes, and reproducibility audits.

Lock updates arrive through reviewed pull requests. Promotion workflows do not mutate `main`.

Only `OrlixOS.xcframework` is a public SDK. Private component OCI artifacts do not become public APIs or SwiftPM products.

## Consequences

Normal integration reuses expensive tested foundations. Every release can still be reconstructed from exact source, locks, tools, and provenance.

Actions caches, Actions artifacts, and Bazel caches remain disposable. They cannot authorize promotion or release.

The exact tested IPA is promoted from TestFlight to App Store review without rebuilding.
