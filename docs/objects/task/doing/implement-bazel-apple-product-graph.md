---
type: task
tags: [task, bazel, apple]
updated: 2026-09-10
status: doing
summary: "Implement explicit Bazel targets for HostAdapter, hosted Kernel composition, native layers, OrlixOS, Orlix, extensions, and tests."
task_of:
  - "[Move the Apple product graph to Bazel](../../story/doing/move-apple-product-graph-to-bazel.md)"
blocks:
  - "[Implement universal Apple Bazel build routing](implement-universal-apple-bazel-build-routing.md)"
---

# Implement The Bazel Apple Product Graph

Determine the HostAdapter composition edge from symbol evidence. Preserve target-specific Swift language modes, all native libraries and frameworks, resources, privacy data, StoreKit configuration, entitlements, deployment targets, and signing behavior. Generate local Xcode projects and commit only the narrow Xcode Cloud bootstrap project.

The app embeds the OrlixOS framework. That framework owns the rootfs images, profile device trees, target-derived plist, and kernel composition record. The existing init component builds OrlixOS `rootinit.c` for `/init` in the initramfs. Kernel actions select the requested profile and Apple destination. Make selects artifacts through configured Bazel queries.

OrlixOSTestApp hosts the existing XCTest suites through Bazel targets and matching Xcode schemes. Building the host or test bundles does not establish conformance. Runtime proof still requires the appropriate upstream test payloads, complete test output, and artifact-bound evidence.
