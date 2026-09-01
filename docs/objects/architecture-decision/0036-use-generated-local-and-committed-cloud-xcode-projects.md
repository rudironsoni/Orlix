---
type: architecture-decision
tags: [architecture, decision, xcode, bazel]
updated: 2026-09-01
status: accepted
external_id: "ADR-0036"
summary: "Generate local Xcode projects from Bazel and commit one minimal Xcode Cloud discovery project that delegates to Make."
part_of:
  - "[Orlix](../product/orlix.md)"
supersedes:
  - "[ADR 0014](0014-use-xcodegen-for-ios-packaging-and-test-harness.md)"
---

# ADR 0036: Use Generated Local And Committed Cloud Xcode Projects

## Status

Accepted.

## Context

Local developers need indexing, navigation, previews, debugging, launch, and focused tests from the Bazel product graph. Xcode Cloud expects a stable project or workspace in the repository for workflow discovery and signing configuration.

## Decision

Local development uses `rules_xcodeproj` to generate disposable worktree-local projects from Bazel targets.

Xcode Cloud uses one committed minimal `OrlixCloud.xcodeproj`. It contains only identifiers required for discovery, shared schemes, signing metadata, and one fixed build phase that invokes a fixed Make target. Make delegates to Bazel. The project does not compile Orlix sources or maintain a second dependency graph.

The migration must prove Xcode 26.6 generation, indexing, Swift and Objective-C navigation, C header navigation, breakpoints, LLDB attachment, previews, test discovery and selection, simulator and device launch, archive, signing, extensions, resources, asset catalogs, privacy data, StoreKit configuration, and Metal compilation.

## Consequences

XcodeGen stops being a build authority after cutover. The committed cloud project is a narrow integration exception, not a competing product graph.

The migration stops before broad conversion if the stable Bazel Apple rules require a permanent fork for Xcode 26.6 or a critical current dependency.
