---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-16
status: doing
summary: "Complete the selected TCTI proof ladder through the pinned app-hosted simulator."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
blocks:
  - "[Integrate and validate the Herdr terminal platform](../todo/integrate-and-validate-herdr-terminal-platform.md)"
  - "[Implement namespaced Local Instance lifecycle](../todo/implement-namespaced-local-instance-lifecycle.md)"
  - "[Run authorized TCTI device validation](../todo/run-authorized-tcti-device-validation.md)"
---

# Complete the pinned simulator TCTI ladder

Run the owning suites in promotion order: kernel KUnit, Linux kselftest, upstream mlibc, upstream Coreutils, HostAdapter XCTest, OrlixOS XCTest, native app XCTest, then the complete app-hosted runtime integration suite. The task envelope records scope and order only. Each suite's native result is the evidence.

## Migration ownership

| Retired host-gate responsibility | Owning test surface |
| --- | --- |
| TCTI instruction decoding, structured exits, registers, PC, faults, TLS, guest memory, and cache invalidation | `arch/orlix` KUnit |
| Linux syscall, exec, process, wait, signal, VFS, PTY, and terminal-size behavior | Linux kselftest through the app-hosted kernel |
| OrlixMLibC build, sysdeps, loader, pthread, TLS, and linked Linux UAPI behavior | Upstream mlibc tests built by OrlixMLibC |
| Coreutils command and package behavior | Complete upstream Coreutils tests |
| Private Darwin trap, memory, and source-preserving console transport | OrlixHostAdapter XCTest |
| Payload metadata, distribution policy, Linux session, terminal transport, and OCI import | OrlixOS XCTest |
| Ghostty integration, shell and terminal interaction, OCI lifecycle, and complete app-hosted behavior | Native app and runtime XCTest |
| Gate-report schemas, golden behavioral models, reducer authorization, aggregate readiness markers, and log parsers | Retired without replacement because native owning results are authoritative |
