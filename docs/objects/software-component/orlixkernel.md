---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-09-10
status: active
summary: "Private static upstream Linux port packaged for iOS-hosted execution."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixKernel

Upstream Linux port compiled into private static `OrlixKernel.xcframework` with identifier `com.rudironsoni.orlix.os.kernel`. It is an Apple-native implementation artifact consumed behind OrlixKit, not a public SDK and not a direct app dependency.

OrlixKernel remains the Linux runtime. Its `arch/orlix` TCTI backend must execute the complete pinned 4,350-leaf AArch64 target, including the union of applicable EL0 feature configurations, when iOS cannot execute guest ELF text. The narrower runtime HWCAP and HWCAP2 projection controls only what Linux may advertise, not the completion denominator. Linux still owns memory management, tasks, scheduling, syscalls, VFS, file descriptors, signals, wait and reaping, PTYs, and process semantics. TCTI returns structured execution exits into those existing kernel paths instead of reimplementing them.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
