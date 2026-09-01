---
type: task
tags:
  - task
  - bazel
  - apple
  - build-system
updated: 2026-09-01
status: todo
summary: "Make Bazel the product build authority for every supported Apple target while retaining Make as the public interface."
task_of:
  - "[Route every Apple build through Bazel](../../story/todo/route-every-apple-build-through-bazel.md)"
depends_on:
  - "[Define the supported Apple Bazel build matrix](define-supported-apple-bazel-build-matrix.md)"
  - "[Implement the Bazel Apple product graph](implement-bazel-apple-product-graph.md)"
blocks:
  - "[Prove all supported Apple builds and feature gates](prove-all-supported-apple-builds-and-feature-gates.md)"
---

# Implement Universal Apple Bazel Build Routing

Route every matrix row through the Bazel product graph for iOS, iPadOS, and Apple-silicon macOS. Cover application and public `OrlixOS` SDK builds, private implementation layers, extensions, test apps, unit and UI tests, simulator and device builds, archives, export inputs, and release bundles.

Keep Make as the stable repository-owned interface. Each Make operation delegates to a fixed Bazel target and preserves current command names unless a separate accepted decision changes them. The generated local Xcode project and committed Xcode Cloud project may provide indexing, debugging, previews, signing discovery, and workflow discovery, but neither project may compile sources or select dependencies through a second graph.

Do not make Bazel invoke the repository top-level or component wrapper Makefiles. Bazel may invoke upstream Kbuild, Meson and Ninja, or upstream Autotools and Make inside declared foreign-build actions. Keep Linux ownership, OrlixOS as the sole public SDK, private HostAdapter boundaries, Herdr terminal ownership, and all iOS 15 availability gates unchanged.

Acceptance requires a graph and workflow audit showing no Apple build surface bypasses Bazel, no Xcode phase owns product compilation, every supported matrix row selects the correct target and deployment setting, and private Kernel, mlibc, Coreutils, and HostAdapter products are not exposed as public SDKs.
