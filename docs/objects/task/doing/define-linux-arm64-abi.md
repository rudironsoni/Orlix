---
type: task
tags: [task, linux-arm64, abi]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#233"
summary: "Define the Orlix userspace ABI as upstream Linux ARM64."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
targets:
  - "[OrlixKernel](../../software-component/orlixkernel.md)"
  - "[OrlixTCTI](../../software-component/orlixtcti.md)"
blocks:
  - "[Check Linux ARM64 ABI equivalence](check-linux-arm64-abi-equivalence.md)"
  - "[Generalize distro-neutral root content](generalize-distro-neutral-root-content.md)"
---

# Define Linux ARM64 ABI

Match upstream Linux ARM64 for architecture identity, ELF, auxv, `AT_PLATFORM`, audit and seccomp identity, syscalls, signal frames, TLS, ptrace, core registers, mmap, vDSO, and HWCAP. Keep HWCAP fail-closed to proved TCTI cohorts. Release and development profiles expose one ABI and require no OrlixMLibC, binary rewriting, Orlix ELF note, or Apple-native link.
