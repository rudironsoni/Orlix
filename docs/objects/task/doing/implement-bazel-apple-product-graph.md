---
type: task
tags: [task, bazel, apple]
updated: 2026-09-10
status: doing
summary: "Implement explicit Bazel targets for HostAdapter, Bootloader, Engine, hosted Kernel composition, OrlixKit, OrlixOS, Orlix, extensions, and tests."
task_of:
  - "[Move the Apple product graph to Bazel](../../story/doing/move-apple-product-graph-to-bazel.md)"
blocks:
  - "[Implement universal Apple Bazel build routing](implement-universal-apple-bazel-build-routing.md)"
---

# Implement The Bazel Apple Product Graph

Determine the HostAdapter composition edge from symbol evidence. Preserve target-specific Swift language modes, all native libraries and frameworks, resources, privacy data, StoreKit configuration, entitlements, deployment targets, and signing behavior. Generate local Xcode projects and commit only the narrow Xcode Cloud bootstrap project.

The app embeds OrlixKit. OrlixKit packages the Apple-native Engine, Bootloader, HostAdapter, and Kernel integration together with OrlixOS guest distribution resources. The existing init component builds the guest `/init` for the initramfs. Kernel actions select the requested profile and Apple destination. Make selects artifacts through configured Bazel queries. Guest mlibc, Coreutils, package, and rootfs providers remain resource inputs and are not Apple-native app link dependencies.

The app uses Apple's registered bundle identifier `com.rudironsoni.Orlix`, and the Live Activity extension uses `com.rudironsoni.Orlix.live-activity`. Device builds resolve the installed Development profiles through the upstream `local_provisioning_profile` rule. The app profile includes its existing iCloud container, App Group, Push, and Fonts capabilities. The extension requests its exact application identifier from the team wildcard profile selected by Xcode. Simulator builds do not resolve device profiles. Make verifies bundle signatures and rejects unresolved or mismatched application identifiers. Existing iCloud, keychain, App Group, and widget identifiers remain stable.

The app and extension plists preserve their bundle names, package types, and plist versions. Their iOS signatures include the team entitlement. The iOS app entitlement list matches Xcode's output, which excludes macOS sandbox network entitlements. Make checks this bundle metadata and both signing identities before accepting the device IPA.

The SSH archive action copies the contents behind Bazel source symlinks into separate platform build directories. OpenSSL and libssh2 configuration and compilation must not write into repository inputs or reuse another platform's object files.

OrlixOSTestApp hosts the existing XCTest suites through Bazel targets and matching Xcode schemes. Building the host or test bundles does not establish conformance. Runtime proof still requires the appropriate upstream test payloads, complete test output, and artifact-bound evidence.

The native `OrlixTests` target uses the existing `OrlixTestApp` entry point. Its app module shares the production source and dependency declarations while retaining the test host's Swift 5 settings and separate module output directory. The test host embeds the existing resources and StoreKit configuration. The generated project preserves the `Orlix Tests` scheme. Compilation and executed test results remain separate evidence.

Architecture tests read the checked-out sources through `BUILD_WORKSPACE_DIRECTORY`. Both project generators supply that path. A missing or invalid path fails the test instead of relying on compiler source-path spelling.
