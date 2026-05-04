# Syscall Surface Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Classify the remaining owned syscall surface and close the process-adjacent dispatch and policy gaps that block later milestones from staying Linux-shaped.

**Architecture:** Treat `runtime/syscall.c` as the public Linux syscall gate and make the matrix authoritative by eliminating `missing:unclassified` entries for subsystems already owned by IXLandSystem. Finish the small but high-leverage process and pidfd gaps first, then tighten explicit unsupported or future-backend policy so later milestones inherit a clean syscall boundary.

**Tech Stack:** `runtime/syscall.c`, `kernel/task.c`, `kernel/fork.c`, `kernel/signal.c`, `kernel/ptrace.c`, `fs/fdtable.c`, LinuxKernel syscall contracts, syscall matrix generator.

---

## Tranche Scope

- `pidfd_send_signal`
- `pidfd_getfd`
- `clone3` `set_tid`
- `unshare(CLONE_FS)` policy
- syscall wrappers already owned by existing kernel subsystems but missing dispatch or policy
- matrix regeneration and inventory-contract updates

### Task 1: Audit And Reclassify The Syscall Matrix

**Files:**
- Modify: `runtime/syscall.c`
- Modify: `docs/syscall_gap_matrix_6.12_arm64.md`
- Reference: `scripts/generate_syscall_gap_matrix.py`
- Test: `IXLandSystemLinuxKernelTests/NativeSyscallContract.c`
- Test: `IXLandSystemLinuxKernelTests/NativeSyscallTests.m`

- [ ] Audit every syscall currently marked `kernel-owned missing:unclassified` and sort each entry into one of four buckets: `implemented:*`, `kernel-owned next:*`, `libc-owned:*`, or `explicit unsupported policy:*`.
- [ ] Extend `NativeSyscallContract.c` with compile-time and runtime inventory expectations for the specific syscalls closed in this milestone.
- [ ] Run the focused inventory proof:

```bash
xcodebuild test \
  -project IXLandSystem.xcodeproj \
  -scheme IXLandSystem-6.12-arm64 \
  -only-testing:IXLandSystemLinuxKernelTests/NativeSyscallTests
```

Expected: the new inventory assertions fail before dispatch changes land.

- [ ] Update `runtime/syscall.c` classification comments and dispatch tables so the matrix generator stops producing stale `unclassified` entries for the audited syscall set.

### Task 2: Close `pidfd_send_signal`

**Files:**
- Modify: `runtime/syscall.c`
- Modify: `kernel/signal.c`
- Modify: `kernel/task.c`
- Test: `IXLandSystemLinuxKernelTests/SignalSyscallContract.c`
- Test: `IXLandSystemLinuxKernelTests/SignalTests.m`

- [ ] Add a failing contract that covers sending a signal through a pidfd, invalid pidfd handling, permission checks, and thread-group versus task targeting semantics.
- [ ] Run the focused signal tests and confirm `-ENOSYS` or current policy failure is visible.
- [ ] Implement the Linux-owner pidfd signal path using existing task lookup and signal-delivery rules instead of duplicating host concepts.
- [ ] Re-run the signal suite until the pidfd path passes and ordinary `kill`/`tgkill` behavior remains green.

### Task 3: Close `pidfd_getfd`

**Files:**
- Modify: `runtime/syscall.c`
- Modify: `fs/fdtable.c`
- Modify: `kernel/task.c`
- Test: `IXLandSystemLinuxKernelTests/FcntlContract.c`
- Test: `IXLandSystemLinuxKernelTests/FcntlTests.m`

- [ ] Add a failing contract for `pidfd_getfd` that checks target-task descriptor lookup, permission rejection, close-on-exec propagation, and bad-target error paths.
- [ ] Run the focused fd suite and capture the failing status.
- [ ] Implement descriptor duplication through the existing fdtable semantics so the result behaves like a Linux-shaped duplicate, not a host fd alias.
- [ ] Re-run the fd suite and verify no regression in `dup`, `dup3`, or `fcntl` duplication behavior.

### Task 4: Finish `clone3` `set_tid` And `unshare(CLONE_FS)` Policy

**Files:**
- Modify: `runtime/syscall.c`
- Modify: `kernel/fork.c`
- Modify: `kernel/task.c`
- Modify: `fs/path.c`
- Test: `IXLandSystemLinuxKernelTests/TaskExecContract.c`
- Test: `IXLandSystemLinuxKernelTests/TaskGroupTests.m`

- [ ] Add failing `clone3` coverage for `set_tid` validation, parent and child bookkeeping, and incompatible flag combinations.
- [ ] Add failing `unshare(CLONE_FS)` coverage that makes the intended policy explicit instead of leaving the syscall unclassified.
- [ ] Implement `clone3` `set_tid` in the task and fork paths with Linux-visible validation rules.
- [ ] Implement either Linux-owner `CLONE_FS` unshare support or a deliberate `-EINVAL` or `-EOPNOTSUPP` policy that is documented in the matrix and tests.
- [ ] Re-run the task and exec suite until both the new behavior and existing clone and exec paths remain green.

### Task 5: Regenerate Matrix And Run Full Proof

**Files:**
- Modify: `docs/syscall_gap_matrix_6.12_arm64.md`

- [ ] Regenerate the matrix:

```bash
python3 scripts/generate_syscall_gap_matrix.py > docs/syscall_gap_matrix_6.12_arm64.md
```

- [ ] Re-run the standard proof gate from the orchestration plan.
- [ ] Run the full LinuxKernel suite after the focused syscall tranche passes.
- [ ] Commit and push only after `HEAD` and `origin/main` match on the verified branch tip.
