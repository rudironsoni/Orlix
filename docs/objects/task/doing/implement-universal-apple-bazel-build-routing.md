---
type: task
tags:
  - task
  - bazel
  - apple
  - build-system
updated: 2026-09-10
status: doing
summary: "Make Bazel the product build authority for every supported Apple target while retaining Make as the public interface."
task_of:
  - "[Route every Apple build through Bazel](../../story/doing/route-every-apple-build-through-bazel.md)"
depends_on:
  - "[Define the supported Apple Bazel build matrix](../done/define-supported-apple-bazel-build-matrix.md)"
  - "[Implement the Bazel Apple product graph](implement-bazel-apple-product-graph.md)"
blocks:
  - "[Prove all supported Apple builds and feature gates](../doing/prove-all-supported-apple-builds-and-feature-gates.md)"
---

# Implement Universal Apple Bazel Build Routing

Route every matrix row through the Bazel product graph for iOS, iPadOS, and Apple-silicon macOS. Cover application and public `OrlixOS` SDK builds, private implementation layers, extensions, test apps, unit and UI tests, simulator and device builds, archives, export inputs, and release bundles.

Keep Make as the stable repository-owned interface. Each Make operation delegates to a fixed Bazel target and preserves current command names unless a separate accepted decision changes them. The generated local Xcode project and committed Xcode Cloud project may provide indexing, debugging, previews, signing discovery, and workflow discovery, but neither project may compile sources or select dependencies through a second graph.

The GNU Make delegation preserves dry-run behavior. Loaded makefiles have
explicit empty recipes so the delegation catch-all cannot treat them as build
goals during makefile regeneration. Recursive delegation passes Make flags to
the required GNU Make version.

The app Make target selects the requested profile, compilation mode, component mode, and iOS destination. The local Xcode project declares Debug and Release configurations. The archive target uses the upstream `rules_apple` `xcarchive` rule and requires archive metadata, debug symbols, the signing team, and a valid code signature before staging. IPA export remains the existing separate Make operation. Signed device builds and device project generation remain gated on compatible app and extension provisioning profiles.

Do not make Bazel invoke the repository top-level or component wrapper Makefiles. Bazel may invoke upstream Kbuild, Meson and Ninja, or upstream Autotools and Make inside declared foreign-build actions. Keep Linux ownership, OrlixOS as the sole public SDK, private HostAdapter boundaries, Herdr terminal ownership, and all iOS 15 availability gates unchanged.

Acceptance requires a graph and workflow audit showing no Apple build surface bypasses Bazel, no Xcode phase owns product compilation, every supported matrix row selects the correct target and deployment setting, and private Kernel, mlibc, Coreutils, and HostAdapter products are not exposed as public SDKs.

With `ORLIX_BAZEL_AUTHORITY=1`, `make app-tests` selects the Bazel-owned native app suite and the existing architecture invariant suite through the shared Make test runner. `ORLIX_APP_TEST_ONLY_TESTING` can select an app test without changing the architecture checks. The default authority remains unchanged until cutover.

Make passes `ORLIX_PINNED_DEVELOPER_DIR` to custom component rules, which set `DEVELOPER_DIR` only for their own actions. Standard Apple actions select both compiler and SDK through Bazel's Xcode version setting. Global `DEVELOPER_DIR` action overrides must not mix the selected compiler with the system-default SDK.
