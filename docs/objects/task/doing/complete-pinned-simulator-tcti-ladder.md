---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-24
status: doing
summary: "Complete the selected TCTI proof ladder through the pinned app-hosted simulator."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
depends_on:
  - "[Complete AArch64 ISA-on-ISA coverage](complete-aarch64-isa-on-isa-coverage.md)"
blocks:
  - "[Integrate and validate the Herdr terminal platform](../todo/integrate-and-validate-herdr-terminal-platform.md)"
  - "[Implement namespaced Local Instance lifecycle](../todo/implement-namespaced-local-instance-lifecycle.md)"
  - "[Run authorized TCTI device validation](../todo/run-authorized-tcti-device-validation.md)"
  - "[Promote TCTI as the product default](../todo/promote-tcti-as-product-default.md)"
---

# Complete the pinned simulator TCTI ladder

Complete target coverage is closed family by family with production-path KUnit and applicable Linux-visible kselftest. Only after that coverage is green does this task rerun the complete app-hosted promotion ladder in ADR 0017 order: kernel dependency proof and KUnit, Linux kernel-interface kselftest, upstream mlibc, OrlixMLibC-built syscall and UAPI kselftest, POSIX shell environment proof, then the jq, curl, and zsh package ladder. HostAdapter XCTest, OrlixOS XCTest, native app XCTest, and complete app-hosted runtime integration prove their owning integration boundaries without replacing any earlier tier. The task envelope records scope and order only. Each suite's native result is the evidence.

## Migration ownership

| Retired host-gate responsibility | Owning test surface |
| --- | --- |
| TCTI instruction decoding, structured exits, registers, PC, faults, TLS, guest memory, and cache invalidation | `arch/orlix` KUnit |
| Linux syscall, exec, process, wait, signal, VFS, PTY, and terminal-size behavior before libc promotion | Linux kernel-interface kselftest through the app-hosted kernel |
| OrlixMLibC build, sysdeps, loader, pthread, and TLS behavior | Upstream mlibc tests built by OrlixMLibC |
| OrlixMLibC-linked Linux syscall and UAPI behavior after mlibc promotion | OrlixMLibC-built Linux kselftest |
| POSIX shell, then jq, curl, and zsh package behavior | Owning shell and package suites through the normal app-hosted Linux path |
| Private Darwin trap, memory, and source-preserving console transport | OrlixHostAdapter XCTest |
| Payload metadata, distribution policy, Linux session, terminal transport, and OCI import | OrlixOS XCTest |
| Ghostty integration, shell and terminal interaction, OCI lifecycle, and complete app-hosted behavior | Native app and runtime XCTest |
| Gate-report schemas, golden behavioral models, reducer authorization, aggregate readiness markers, and log parsers | Retired without replacement because native owning results are authoritative |
