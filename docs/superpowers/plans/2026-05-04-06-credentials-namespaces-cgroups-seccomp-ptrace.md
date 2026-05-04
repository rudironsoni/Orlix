# Credentials, Namespaces, Cgroups, Seccomp, And Ptrace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the security and isolation tranches that IXLandSystem already owns so credentials, namespaces, cgroups, seccomp, and ptrace expose coherent Linux-visible behavior.

**Architecture:** Keep identity, isolation, and policy state in the credential, namespace, cgroup, seccomp, and ptrace owners, with procfs reflecting that state without importing host vocabulary. The tranche closes lifecycle, reporting, and policy holes by writing contract-first tests for visible state transitions and then filling only the Linux-owner behavior needed by the owned syscall surface.

**Tech Stack:** `kernel/cred.c`, `kernel/cred_internal.h`, `kernel/cgroup.c`, `kernel/cgroup.h`, `kernel/seccomp.c`, `kernel/seccomp.h`, `kernel/ptrace.c`, `kernel/uts.c`, procfs reporting tests, namespace and cgroup tests.

---

## Tranche Scope

- saved IDs and filesystem IDs where required
- `no_new_privs`
- capability checks in namespace context
- namespace lifecycle and proc visibility
- cgroup lifecycle, accounting, migration, and namespace views
- seccomp policy completeness for owned scope
- ptrace scope and explicit unsupported policy where required

### Task 1: Complete Credential Depth

**Files:**
- Modify: `kernel/cred.c`
- Modify: `kernel/cred_internal.h`
- Modify: `runtime/syscall.c`
- Test: `IXLandSystemLinuxKernelTests/CredentialTests.m`
- Test: `IXLandSystemLinuxKernelTests/NativeSyscallContract.c`

- [ ] Add failing coverage for saved IDs, filesystem IDs if surfaced by owned syscalls, and `no_new_privs` transitions through the public syscall boundary.
- [ ] Run credential-focused tests and capture the current mismatch.
- [ ] Implement the minimal credential-owner updates needed for Linux-visible identity semantics.
- [ ] Re-run credential tests until the new identity paths and old setuid or setgid behavior remain green.

### Task 2: Finish Namespace Lifecycle And Proc Visibility

**Files:**
- Modify: namespace-related kernel sources under `kernel/`
- Modify: procfs namespace reporting source under `fs/`
- Test: `IXLandSystemLinuxKernelTests/NamespaceContract.c`
- Test: `IXLandSystemLinuxKernelTests/NamespaceTests.m`
- Test: `IXLandSystemLinuxKernelTests/ProcfsNamespaceContract.c`
- Test: `IXLandSystemLinuxKernelTests/ProcfsNamespaceTests.m`

- [ ] Add failing coverage for mount, PID, UTS, user, and cgroup namespace lifecycle, ownership, rebasing, and `/proc` visibility.
- [ ] Run the namespace and procfs namespace suites to capture the current lifecycle gaps.
- [ ] Implement the minimal namespace-owner changes needed for coherent Linux-visible views.
- [ ] Re-run both suites until task-visible namespace state and proc reporting remain green.

### Task 3: Deepen Cgroup Semantics

**Files:**
- Modify: `kernel/cgroup.c`
- Modify: `kernel/cgroup.h`
- Modify: cgroup-related proc or fs reporting source under `fs/`
- Test: `IXLandSystemLinuxKernelTests/CgroupContract.c`
- Test: `IXLandSystemLinuxKernelTests/CgroupTests.m`

- [ ] Add failing coverage for cgroup lifecycle, task migration rules, accounting, event reporting, and namespace interaction.
- [ ] Run cgroup tests to capture current incomplete behavior.
- [ ] Implement the minimal cgroup-owner updates needed for Linux-shaped lifecycle and visibility.
- [ ] Re-run cgroup tests until the new cases and prior cgroup behavior both pass.

### Task 4: Finalize Seccomp And Ptrace Scope Decisions

**Files:**
- Modify: `kernel/seccomp.c`
- Modify: `kernel/seccomp.h`
- Modify: `kernel/ptrace.c`
- Modify: `runtime/syscall.c`
- Test: `IXLandSystemLinuxKernelTests/SeccompContract.c`
- Test: `IXLandSystemLinuxKernelTests/SeccompTests.m`
- Test: `IXLandSystemLinuxKernelTests/PtraceContract.c`
- Test: `IXLandSystemLinuxKernelTests/PtraceTests.m`

- [ ] Add failing coverage for every seccomp action or status path IXLandSystem claims to own, plus explicit unsupported-policy assertions for out-of-scope behavior.
- [ ] Add ptrace-scope assertions if milestone-03 left follow-up policy or reporting gaps.
- [ ] Run seccomp and ptrace suites and capture the failing policy or lifecycle details.
- [ ] Implement the minimal Linux-owner policy completion needed for a coherent public contract.
- [ ] Re-run seccomp and ptrace suites until the owned scope is fully covered by tests.

### Task 5: Full Tranche Proof

**Files:**
- Reference: `docs/superpowers/plans/2026-05-04-00-kernel-completion-orchestration.md`

- [ ] Re-run lint, project generation, and the AGENTS-authoritative simulator `build-for-testing` flow.
- [ ] Run the focused credential or namespace or cgroup or seccomp or ptrace simulator suites for tranche-local proof, then run the full shared-scheme simulator suite before any milestone-finished claim.
- [ ] Regenerate the syscall matrix if credential or policy syscall classifications changed.
- [ ] Commit and push only after the proof gate passes and branch synchronization is verified.
