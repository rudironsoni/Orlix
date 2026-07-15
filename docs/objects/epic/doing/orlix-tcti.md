---
type: epic
tags:
  - epic
  - orlix-tcti
updated: 2026-07-15
status: doing
summary: "Complete safe, conformant hosted Linux ELF execution through TCTI."
targets:
  - "[Orlix](../../product/orlix.md)"
  - "[TCTI](../../software-component/tcti.md)"
has_story:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
---

# Orlix TCTI

Complete safe, conformant hosted Linux ELF execution through TCTI.

TCTI executes ordinary unmodified AArch64 Linux ELF user instructions until Linux needs control again. OrlixKernel remains upstream Linux, owns the Linux UAPI and process model, and dispatches syscalls. TCTI lives under `arch/orlix`; OrlixHostAdapter may provide private host memory, console, timer, lifecycle, and device mechanics but cannot decode Linux syscall policy or own VFS, fd, signal, wait, exec, process, or page-table semantics.

Guest ELF text remains host data. Product paths cannot request executable protection for guest text, JIT, `MAP_JIT`, RWX, or generated executable memory. Simulator and physical-device validation use the same TCTI backend and executable-memory restrictions. Direct-native guest execution is limited to a low-level oracle or benchmark and cannot satisfy product gates.

Proof advances from seed probes through kernel, kselftest, OrlixMLibC, OrlixMLibC-linked UAPI, shell, Coreutils, OCI, app-hosted simulator, device, and release tiers. Golden ELFs and reducers prove narrow behavior only. Phone work remains ineligible until the complete pinned-simulator readiness ladder passes. The product default cannot flip until real `/init` reaches `svc #0`, enters Linux syscall dispatch, emits Linux console output through the app-hosted path, and every forbidden-behavior check is false.

The autonomous entry points are `make agent-status AREA=orlix-tcti`, `make agent-next AREA=orlix-tcti`, and `make agent-task-envelope-check AREA=orlix-tcti`. The selected task, allowed scope, prerequisites, proof tier, classifier state, and exact evidence paths belong to `Build/AgentHarness/orlix-tcti/next-task.json` and the current reports. This page does not copy their status.

## Autonomous Test Contract

The harness uses stable gate IDs, commands, proof tiers, acceptance weights, real-stack requirements, and machine-readable results.

## Failure Reduction

A failing higher-tier gate selects a reproducible reducer before production code changes. Narrow reducer success does not satisfy the failed product gate.

## TCTI Concurrency Model

The translated user-instruction backend preserves Linux task and thread ownership. Host concurrency cannot invent a second process model.

## TPIDR_EL0 Transition Audit

Thread-pointer transitions are explicit TCTI correctness and safety boundaries.

## Virtio Boundary

Virtio carries device mechanics. It does not carry Linux process, syscall, signal, or memory-management policy.

The durable implementation contract includes the `tcti-appstore-safety-audit` gate and `orlix-aarch64-v1` guest profile.

Orlix TCTI is an Orlix-owned, arch/orlix, no-JIT, same-ISA, tail-call-threaded user-instruction backend for unmodified AArch64 Linux ELF binaries. It does not replace Linux; it lets OrlixKernel’s existing Linux userspace surface run on iOS without host-executable guest text.
