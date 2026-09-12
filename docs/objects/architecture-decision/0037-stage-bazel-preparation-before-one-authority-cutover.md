---
type: architecture-decision
tags: [architecture, decision, bazel, migration]
updated: 2026-09-10
status: accepted
external_id: "ADR-0037"
summary: "Merge inert Bazel preparation in reviewable units and switch product build authority in one final cutover."
part_of:
  - "[Orlix](../product/orlix.md)"
amended_by:
  - "[ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md)"
---

# ADR 0037: Stage Bazel Preparation Before One Authority Cutover

## Status

Accepted.

## Context

A single months-long migration branch would drift while Kernel, TCTI, rootfs, release, and native app work continues. A partial authority migration would leave Bazel, XcodeGen, and Make competing for product truth.

## Decision

Merge inert preparation as ordered, reviewable changes on `main`: Bzlmod bootstrap, toolchain definitions, providers, custom rules, shadow targets, manifests, benchmarks, generated-project experiments, and non-required parity CI.

For [rudironsoni/Orlix#228](https://github.com/rudironsoni/Orlix/pull/228), the complete recovery remains on the existing `fix/build-optimizations` branch. Implement coherent, independently verified checkpoints there without coupling unrelated commits or verification steps. Each checkpoint MUST record its authority changes, evidence, and unresolved gates in `IMPLEMENT.md`. Rudi performs the merge.

Keep the current Make and Xcode authority active during preparation. The final cutover changes Make delegates, Xcode ownership, required checks, artifact selection, and release workflows together. It then removes legacy component orchestration, build-owning Xcode phases, timestamp identities, XcodeGen authority, and duplicate vendor paths.

Freeze conflicting toolchain and upstream upgrades during final parity. Tag the last known-good pre-Bazel commit and keep one read-only rollback branch for one shipped release cycle and one clean reconstruction exercise.

## Consequences

Preparatory work remains reviewable and current with product development. The repository still has one product build authority before and after the final switch.

No preparation checkpoint or inert target can authorize promotion or release before parity, proof, Apple gates, cache equivalence, invalidation locality, developer-loop benchmarks, and the one final authority cutover pass.
