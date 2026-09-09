---
type: task
tags: [task, bazel, apple]
updated: 2026-09-09
status: doing
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

Fail the task when support requires a permanent fork of Bazel Apple rules.
