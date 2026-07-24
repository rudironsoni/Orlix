---
type: epic
tags:
  - epic
  - orlix-tcti
updated: 2026-07-24
status: doing
summary: "Complete safe, conformant hosted Linux ELF execution through TCTI."
targets:
  - "[Orlix](../../product/orlix.md)"
  - "[TCTI](../../software-component/tcti.md)"
has_story:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
blocks:
  - "[OCI-derived environments](oci-derived-environments.md)"
  - "[Orlix release](orlix-release.md)"
---

# Orlix TCTI

Complete safe, conformant hosted Linux ELF execution through TCTI.

TCTI executes ordinary unmodified AArch64 Linux ELF user instructions until Linux needs control again. OrlixKernel remains upstream Linux, owns the Linux UAPI and process model, and dispatches syscalls. TCTI lives under `arch/orlix`; OrlixHostAdapter may provide private host memory, console, timer, lifecycle, and device mechanics but cannot decode Linux syscall policy or own VFS, fd, signal, wait, exec, process, or page-table semantics.

Guest ELF text remains host data. Product paths cannot request executable protection for guest text, JIT, `MAP_JIT`, RWX, or generated executable memory. Simulator and physical-device validation use the same TCTI backend and executable-memory restrictions. Direct-native guest execution is limited to a low-level oracle or benchmark and cannot satisfy product gates.

Proof advances in ADR 0017 order through kernel dependency proof and KUnit, Linux kernel-interface kselftest, upstream mlibc tests, OrlixMLibC-built syscall and UAPI proof, a POSIX shell environment, then jq, curl, and zsh package proof. Coreutils, HostAdapter XCTest, OrlixOS XCTest, native app XCTest, the complete app-hosted simulator suite, device proof when selected, and release tiers prove their own boundaries without replacing an earlier tier. Phone work remains ineligible until the complete pinned-simulator readiness ladder passes. The product default cannot flip until real `/init` reaches `svc #0`, enters Linux syscall dispatch, emits Linux console output through the app-hosted path, and every forbidden-behavior check is false.

The scope-envelope entry points are `make agent-status AREA=orlix-tcti`, `make agent-next AREA=orlix-tcti`, and `make agent-task-envelope-check AREA=orlix-tcti`. The envelope records owning suites and their order. It does not execute tests, interpret native results, or claim readiness.

The durable completion boundary is the complete pinned AArch64 target inventory, independent of the narrower runtime HWCAP profile. Every Arm source leaf must be explicitly classified, every applicable EL0 feature variant remains blocking until its production semantics and typed evidence are complete, and every privileged leaf requires its architecturally correct EL0 behavior. Runtime HWCAP and HWCAP2 describe only capabilities already proved from that complete target. Workload traces may order the work, but passing a workload or withholding a capability cannot retire missing instruction families or justify reduced semantics.

## Owning Test Contract

Each layer owns its assertions and native result format. TCTI engine correctness belongs to KUnit, Linux-visible behavior belongs to kselftest, libc and package behavior belong to their upstream suites, private Darwin transport belongs to HostAdapter XCTest, and product integration belongs to OrlixOS or native app XCTest.

## Failure Reduction

A failing assertion is minimized into the same owning suite. A host-side behavioral model, terminal-text parser, or cross-layer aggregate cannot replace the failed test.

## TCTI Concurrency Model

The translated user-instruction backend preserves Linux task and thread ownership. Host concurrency cannot invent a second process model.

## TPIDR_EL0 Transition Audit

Thread-pointer transitions are explicit TCTI correctness and safety boundaries.

## Virtio Boundary

Virtio carries device mechanics. It does not carry Linux process, syscall, signal, or memory-management policy.

The durable implementation contract includes App Store executable-memory invariants in native XCTest.

Orlix TCTI is an Orlix-owned, arch/orlix, no-JIT, same-ISA, tail-call-threaded user-instruction backend for unmodified AArch64 Linux ELF binaries. It does not replace Linux; it lets OrlixKernel’s existing Linux userspace surface run on iOS without host-executable guest text.
