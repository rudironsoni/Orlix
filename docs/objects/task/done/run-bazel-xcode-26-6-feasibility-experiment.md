---
type: task
tags: [task, bazel, apple]
updated: 2026-09-15
status: done
summary: "Run the bounded Bazel 9.2.0 and Xcode 26.6 compatibility experiment and record every supported or blocked behavior."
task_of:
  - "[Prove the Bazel Apple feasibility gate](../../story/doing/prove-bazel-apple-feasibility-gate.md)"
blocks:
  - "[Define the supported Apple Bazel build matrix](../done/define-supported-apple-bazel-build-matrix.md)"
---

# Run The Bazel Xcode 26.6 Feasibility Experiment

Bootstrap the pinned stable module matrix. Prove the iOS 15.0 app and public SDK minimum, all current Swift packages, Ghostty, libssh2, OpenSSL, Metal, the optional iOS 16.1 live activity extension, simulator and device builds, archive and signing, indexing, debugging, previews, test selection, one Kernel archive and installed-UAPI output, mlibc from that UAPI, and the minimal committed Xcode Cloud project shape.

The dedicated GitHub CI lane installs iOS 15.5 with verified `xcodes`, creates
an isolated simulator, and runs the Make-owned app launch test. Local runtime
availability cannot weaken or remove this proof requirement.

The MLX package builds its pinned upstream Metal sources into
`mlx-swift_Cmlx.bundle/default.metallib`, the bundle expected by MLX. The app
packaging check requires that library. Compiler-only bridging headers remain
compiler inputs and must not enter app resource processing. The existing native
smoke suite loads the compiled library through Metal and resolves an upstream
kernel. This proves shader packaging and loading, not MLX model execution.
The shared Make test runner requires executed tests with zero failures and skips.

Fail the task when support requires a permanent fork of Bazel Apple rules.

2026-09-15: the experiment completed. Canonical Apple CI proved the matrix at
Xcode 26.6 build 17F113, and protected promotion verified the full
seven-component buildset under the same pin. The product pin then briefly
moved to Xcode 27.0 build 27A266a, which required reworking the kernel product
link to survive the removed `ld-classic`; the GitHub runner image cannot
select Xcode 27.0, so the product pin returned to Xcode 26.6 and Xcode 27.0
stays an allowed local identity. The linker-agnostic kernel ordering works
under both toolchains.
