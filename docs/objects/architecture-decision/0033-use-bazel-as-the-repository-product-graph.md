---
type: architecture-decision
tags: [architecture, decision, bazel, build-system]
updated: 2026-09-01
status: accepted
external_id: "ADR-0033"
summary: "Use Bazel as the repository product graph while Make remains the supported interface and upstream build engines retain internal ownership."
part_of:
  - "[Orlix](../product/orlix.md)"
amends:
  - "[ADR 0019](0019-keep-make-targets-linux-shaped.md)"
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

Apple compilation, linking, resources, tests, packaging, Xcode project generation, rootfs assembly, manifests, proof selection, and release bundles become Bazel-declared product graph operations.

Apple compilation mode, Orlix profile, destination, source or promoted component mode, signing mode, and proof tier remain independent settings.

## Consequences

The final repository has one cross-component product graph without taking ownership of Linux, libc, or package semantics.

The current Make command names remain stable unless a separate accepted decision changes the public interface. Ordinary `make build` does not call `clean`.

The migration must use narrow providers and the correct split between analysis, execution, workflow-policy, and fault-injection tests.
