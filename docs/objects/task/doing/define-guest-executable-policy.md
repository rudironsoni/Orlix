---
type: task
tags: [task, app-store, linux-arm64]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#232"
summary: "Classify Linux guest executable content separately from Apple-native executable code."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
targets:
  - "[Orlix](../../product/orlix.md)"
  - "[Release harness](../../software-component/release-harness.md)"
derived_from:
  - "[ADR 0023](../../architecture-decision/0023-use-release-development-profiles-and-curated-orlixos-distribution.md)"
  - "[ADR 0027](../../architecture-decision/0027-use-app-store-only-cross-platform-host-integration.md)"
blocks:
  - "[Prove native Linux package managers](prove-native-linux-package-managers.md)"
---

# Define guest executable policy

Classify Linux ELF, scripts, bytecode, OCI layers, and rootfs content as host data executed only through upstream Linux and OrlixTCTI. Keep Apple-native extension code, runtime native code generation, JIT, `MAP_JIT`, and RWX mappings outside that domain. Reconcile ADR 0023, ADR 0027, product capability, provenance, and release gates without exposing Apple APIs as guest ABI.
