# Kernel Completion Orchestration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the remaining IXLandSystem kernel milestones in a fixed dependency order without violating the Linux-owner versus host-mediation boundary.

**Architecture:** Drive the remaining work as seven bounded subsystem tranches with one orchestration plan enforcing sequencing, proof, and rollback rules. Every milestone proves Linux-visible behavior through LinuxKernel contracts first, adds HostBridge proof only when `internal/ios/**` seams change, and refuses completion claims until the full LinuxKernel suite and branch-state checks are green.

**Tech Stack:** C and Objective-C kernel/runtime code, vendored Linux UAPI 6.12 arm64, XcodeGen, XcodeBuildMCP CLI, LinuxKernel contract tests, HostBridge seam tests, syscall gap matrix generation.

---

## Dependency Graph

```text
00 orchestration
  -> 01 syscall surface closure
  -> 02 VFS / mounts / procfs / devfs
  -> 03 task / clone / exec / wait / signals
  -> 04 memory / futex
  -> 05 readiness / TTY
  -> 06 credentials / namespaces / cgroups / seccomp / ptrace
  -> 07 virtual networking
```

## Shared Constraints

- Linux semantic ownership stays in `fs/`, `kernel/`, `runtime/`, and `include/`.
- Host mechanics stay private under `internal/ios/**`; no ambient host vocabulary leaks into Linux-owner files.
- LinuxKernel tests prove Linux-visible behavior with syscall-facing contracts and C UAPI checks only.
- HostBridge tests are supplemental seam proof only; they never count as Linux semantics proof.
- No milestone is complete on lint alone, on build alone, or on one narrow green test.
- Each milestone ends with proof on iOS Simulator, commit, push, and `HEAD == origin/main`.

## Carry-Forward Baseline

- Shell-base syscall tranche is already closed through `pidfd_open`.
- Remaining process-surface gaps explicitly called out for early closure are `pidfd_send_signal`, `pidfd_getfd`, `clone3` `set_tid`, and `unshare(CLONE_FS)` policy.
- Current large remaining tranches are VFS/mount depth, task and signal parity, VM/futex depth, readiness and TTY hardening, credentials and namespaces expansion, and virtual networking.
- `docs/syscall_gap_matrix_6.12_arm64.md` still carries many `kernel-owned missing:unclassified` entries that must be classified milestone by milestone.

### Task 1: Lock Milestone Order And Branch Policy

**Files:**
- Modify: `docs/superpowers/plans/2026-05-04-00-kernel-completion-orchestration.md`
- Reference: `AGENTS.md`
- Reference: `docs/syscall_gap_matrix_6.12_arm64.md`

- [ ] Confirm the fixed milestone order above and do not start a later milestone before the earlier milestone is merged and `origin/main` matches local `HEAD`.
- [ ] Refuse mixed-subsystem work. If a change request touches a second milestone, stop and write a follow-up plan instead of widening the active tranche.
- [ ] Default isolation rule: use one branch or worktree per active milestone and close it only after proof is complete. Only an explicit user-approved exception may allow work on `main` without a milestone branch or worktree.
- [ ] Record the current exception explicitly when it exists. For this plan, Task 1 may update the orchestration document directly on `main` by prior user approval, but that exception does not relax the default branch/worktree rule for milestones `01` through `07`.
- [ ] Record every proof run in the milestone plan checklist before committing.

### Task 2: Enforce The Shared Proof Contract

**Files:**
- Reference: `scripts/lint_linux_surface.sh`
- Reference: `IXLandSystem.xcodeproj`
- Reference: `IXLandSystemLinuxKernelTests`
- Reference: `IXLandSystemHostBridgeTests`

- [ ] Run Linux-owner lint first:

```bash
rtk bash ./scripts/lint_linux_surface.sh
```

Expected: exit status `0` and no new Linux-owner host-leak findings.

- [ ] Regenerate the project before simulator proof:

```bash
rtk xcodegen generate --project .
```

Expected: `IXLandSystem.xcodeproj` regenerates without errors.

- [ ] Build for testing on iPhone 17 through the AGENTS-authoritative simulator flow:

```bash
rtk xcodebuild build-for-testing \
  -project IXLandSystem.xcodeproj \
  -scheme IXLandSystem-6.12-arm64 \
  -sdk iphonesimulator \
  -configuration Debug \
  -destination 'platform=iOS Simulator,name=iPhone 17'
```

Expected: build succeeds with no new warnings that indicate boundary regressions. If CoreSimulator is unavailable on the worker, record that infrastructure block explicitly; do not replace this step with an `xcodebuildmcp simulator build` argument shape that the local tool does not accept. For milestones `01` through `07`, completion remains blocked until this AGENTS-required iOS Simulator `build-for-testing` step is actually green.

- [ ] For milestone work that changes kernel behavior, run the focused LinuxKernel tranche tests for the active milestone before the full suite.
- [ ] For milestone work that changes kernel behavior, run the full LinuxKernel suite after the focused tranche passes. Orchestration-only or documentation-only Task `00` work is exempt from both LinuxKernel test bullets only when lint and project generation are green, the simulator step has been attempted with the AGENTS-required command, and the only blocker is documented simulator infrastructure unavailability; a real `build-for-testing` failure does not satisfy this exception.
- [ ] Run HostBridge tests only if the milestone adds or widens an `internal/ios/**` seam.

### Task 3: Standard Implementation Skeleton For Every Milestone

**Files:**
- Reference: `IXLandSystemLinuxKernelTests/*.c`
- Reference: `IXLandSystemLinuxKernelTests/*.m`
- Reference: `runtime/syscall.c`
- Reference: milestone-owned source files under `fs/`, `kernel/`, `runtime/`, `include/`

- [ ] Write or extend the failing LinuxKernel contract first using a C contract file for Linux UAPI constants, macros, structs, and ioctl payload truth, and use an Objective-C test only for syscall-visible assertions.
- [ ] Keep LinuxKernel test ownership clean: Objective-C test files must not include Linux UAPI headers, and LinuxKernel tests must not include `internal/ios/**` or host-only proof helpers.
- [ ] Run the focused contract test alone and capture the failing symptom.
- [ ] Implement the minimal Linux-owner code needed to satisfy the contract in milestone-owned Linux paths under `fs/`, `kernel/`, `runtime/`, or `include/`.
- [ ] If host mediation is required, add or adjust only a narrow subsystem-owned seam under `internal/ios/**`; do not move Linux semantic decisions, Linux-visible ABI truth, or Darwin-shaped behavior into that seam.
- [ ] Re-run the focused test until it passes.
- [ ] Re-run the full LinuxKernel suite before any commit claim.
- [ ] Re-run lint and simulator build if the implementation touched shared headers, syscall dispatch, or broad subsystem paths.

### Task 4: Matrix And Boundary Audits

**Files:**
- Modify: `docs/syscall_gap_matrix_6.12_arm64.md`
- Reference: `scripts/generate_syscall_gap_matrix.py`
- Reference: `runtime/syscall.c`

- [ ] Apply this task to milestone implementation tranches `01` through `07` when they change syscall surface, Linux-owner source, or LinuxKernel tests. Orchestration-only Task `00` documentation work does not regenerate the matrix or run these audits unless it also changes one of those milestone-owned surfaces.

- [ ] Regenerate the syscall matrix after every syscall-surface change that lands within milestone tranches `01` through `07`; do not defer regeneration to a later milestone checkpoint:

```bash
rtk python3 scripts/generate_syscall_gap_matrix.py > docs/syscall_gap_matrix_6.12_arm64.md
```

Expected: touched syscalls move out of `missing:unclassified` into `implemented:*`, `explicit unsupported policy:*`, `libc-owned:*`, or `future backend:*`.

- [ ] Audit only the changed milestone Linux-owner files for Darwin or iOS names. Pass the explicit changed-file list under `fs/`, `kernel/`, `runtime/`, and `include/`; do not scan untouched trees:

```bash
rtk rg -n "darwin|TargetConditionals|Foundation|NS[A-Z]|dispatch_|pthread_|mach_|__APPLE__|TARGET_OS_" <changed-linux-owner-files>
```

Expected: no new matches introduced by the milestone.

- [ ] Audit only the changed milestone LinuxKernel test files for forbidden host-only families such as `internal/ios/**`, `TargetConditionals`, `dispatch`, `pthread`, `mach`, `Foundation`, and `NS*`, plus branded helper families forbidden by `AGENTS.md`; do not scan untouched test files:

```bash
rtk rg -n "internal/ios/|TargetConditionals|dispatch_|pthread_|mach_|Foundation|NS[A-Z]|ixland_test_|IX_|TEST_" <changed-linuxkernel-test-files>
```

Expected: no LinuxKernel test picks up new host-only headers or branded helpers.

### Task 5: Commit, Push, And Rollback Rules

**Files:**
- Reference: `.git`

- [ ] Commit only after the proof contract is complete and recorded in the active milestone plan.
- [ ] Keep the default branch/worktree rule from Task 1: milestones `01` through `07` commit on their active milestone branch or worktree branch, not on `main`. The only allowed direct-to-`main` case is an explicit user-approved exception recorded in the plan, such as this Task `00` orchestration-doc update.
- [ ] Use milestone-scoped commit messages such as `kernel: close pidfd and clone3 syscall gaps` or `fs: deepen procfs and mount parity`. For an approved orchestration-only Task `00` edit on `main`, use a docs-scoped message that names the orchestration change directly.
- [ ] Push immediately after the proof-backed commit to the active branch. A pushed milestone branch is not a completion signal by itself; milestone completion still requires merge or fast-forward onto `main` and the synchronization check below.
- [ ] Verify branch synchronization:

```bash
rtk git rev-parse HEAD
rtk git rev-parse origin/main
```

Expected: both hashes match only when the milestone is actually complete on `main`. For the default milestone-branch flow, run this after the proof-backed branch is merged or fast-forwarded onto `main` and local `HEAD` is updated to that same tip. For an explicit user-approved direct-to-`main` exception, the same hash match is still required before completion is recorded.

- [ ] If proof fails after a local commit, fix forward in the same active branch, or on `main` only when the user explicitly approved that exception for the current task scope. Do not rewrite shared history, force-push, or revert unrelated user work, and do not weaken lint or tests to force green.
