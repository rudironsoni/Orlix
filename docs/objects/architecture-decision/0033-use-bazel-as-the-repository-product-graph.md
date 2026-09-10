---
type: architecture-decision
tags: [architecture, decision, bazel, build-system]
updated: 2026-09-10
status: accepted
external_id: "ADR-0033"
summary: "Use Bazel as the repository product graph while Make remains the supported interface and upstream build engines retain internal ownership."
part_of:
  - "[Orlix](../product/orlix.md)"
amends:
  - "[ADR 0019](0019-keep-make-targets-linux-shaped.md)"
amended_by:
  - "[ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md)"
---

# ADR 0033: Use Bazel As The Repository Product Graph

## Status

Accepted.

## Context

Orlix currently spreads cross-component dependency discovery, source preparation, packaging, proof orchestration, Xcode integration, and release identity across repository Makefiles, XcodeGen, Xcode phases, stamps, and workflows. Expensive component work is difficult to reuse safely across developers, worktrees, and CI.

Linux Kbuild, mlibc Meson and Ninja, and imported Autotools and Make projects already define correct dependencies inside their upstream components. Reimplementing those definitions in Starlark would create competing upstream semantics.

## Decision

Bazel is the repository-level product graph, cross-component dependency graph, test-selection graph, packaging graph, and artifact identity authority after cutover.

Make remains the only supported repository-owned developer and CI command interface. Make delegates fixed product operations to Bazel. Direct Bazel commands remain maintainer diagnostics.

Kbuild, Meson and Ninja, and upstream Autotools and Make remain authoritative inside their components. Bazel invokes these engines directly with pinned tools, prepared source trees, declared inputs, narrow typed providers, and declared outputs. Bazel must not call the top-level Orlix Makefile or component wrapper Makefiles from a Bazel action.

Apple-native compilation, linking, resources, tests, packaging, Xcode project generation, guest distribution assembly, manifests, proof selection, and release bundles become Bazel-declared product graph operations. OrlixKit is the public Apple product boundary. OrlixEngine, OrlixBootloader, OrlixHostAdapter, and Kernel Mach-O integration are native implementation layers. OrlixMLibC, OrlixCoreUtils, guest packages, and rootfs/images remain Linux guest or distribution artifacts and MUST NOT be hidden as Apple-native link dependencies.

Cross-component actions exchange narrow semantic product artifacts. Consumers MUST select specific provider fields rather than inherit a producer's complete `DefaultInfo` output set. Provenance, source manifests, proof records, and source identities MUST NOT invalidate downstream compilation unless their contents semantically affect that compilation.

Action identity contains every effective input that can change an output. Incremental-directory identity selects compatible mutable upstream state and remains stable across ordinary source edits. Artifact/content identity describes real output content and metadata. Proof/provenance identity binds evidence to tested artifacts and policy without becoming a compile input.

Every Apple product build surface uses this Bazel graph for every row in the supported matrix, including iOS and iPadOS 15, later iOS and iPadOS versions, the optional iOS and iPadOS 16.1 Live Activity extension, and every supported Apple-silicon macOS version. A destination or OS version is a declared graph dimension, not a reason to add an XcodeGen, direct `xcodebuild`, or other parallel build authority. Local Xcode projects, the committed Xcode Cloud discovery project, and Make are frontends to the same Bazel graph.

Apple compilation mode, Orlix profile, destination, source or promoted component mode, signing mode, and proof tier remain independent settings.

## Consequences

The final repository has one cross-component product graph without taking ownership of Linux, libc, or package semantics.

The current Make command names remain stable unless a separate accepted decision changes the public interface. Ordinary `make build` does not call `clean`.

The migration must use narrow providers and the correct split between analysis, execution, workflow-policy, and fault-injection tests.

No Apple OS version becomes supported until its matrix row has a Bazel route, valid cache or promoted-buildset identity, and the required compile, link, runtime, and feature-gate evidence. The iOS 15 reduced feature set remains supported, while later-system features remain gated on rows where their APIs are unavailable.
