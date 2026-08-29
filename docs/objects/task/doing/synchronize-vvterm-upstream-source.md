---
type: task
tags:
  - task
  - native-app-foundation
  - upstream
updated: 2026-08-29
status: doing
summary: "Synchronize the tracked Orlix application with an exact vvterm source snapshot without importing upstream history."
task_of:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
derived_from:
  - "[ADR 0032](../../architecture-decision/0032-sync-vvterm-with-three-way-source-snapshots.md)"
---

# Synchronize vvterm upstream source

The source pin advanced from `791eebae946b0831ffff3ac839e0f2b75d076458` to `31120756133d22e526d01c630c680bd949dda730`. The new upstream tree is `f0e529842c8f967a5f19748a63399d276215092e`.

The repository now owns a three-way snapshot sync, a versioned branding policy, an audited conflict-resolution manifest, exact source provenance, and source-level regression tests. The update keeps the OrlixOS local terminal and routes imported analytics events through Orlix telemetry instead of SwiftUmami.

The source sync tests, branding check, Xcode project generation, and release-input tests pass. Full app and pristine upstream Xcode tests remain blocked because the installed Xcode instances do not have the optional Metal toolchain component required by MLX. A clean Ghostty rebuild also requires Zig 0.16, while the machine has Zig 0.15.2. The tracked exact `GhosttyKit.xcframework` supplies the compatibility archives without changing their bytes.
