# AGENTS.md - Linux-Shaped Architecture Rules for IXLandSystem

## Project Invariant

IXLandSystem is a Linux-shaped headers + syscall + runtime target hosted on iOS.

If a change makes IXLandSystem less suitable for real Linux userspace (for example bash, zsh, grep, sed, awk, fzf), the change is wrong.

Repo-local convenience never outranks Linux userspace compatibility.

## 1) Linux-Shaped Surface First

Linux-owner behavior must prefer Linux expectations for:
- headers, constants, and public names
- struct layouts and ioctl payload contracts
- errno behavior
- file descriptor and open-file-description semantics
- path resolution, dirfd, and pathname rules
- poll/select readiness behavior
- signals, default actions, masking, and delivery
- sessions/process-groups/controlling-tty behavior
- termios/tty behavior
- procfs/devfs names and shape
- exec/shebang/interpreter/argv/env/wait/exit behavior

Do not drift toward Darwin-shaped semantics in Linux-owner code.

## 2) Ownership and Directionality

Linux-owner paths (Linux semantics live here):
- `fs/`
- `kernel/`
- `runtime/`
- `include/`

Host mediation paths (host mechanics live here only):
- `internal/ios/**`

Wrong-direction changes are forbidden:
- Do not move Linux semantic decisions into `internal/ios/**`.
- Do not move host mechanics into Linux-owner paths.

## 3) Narrow Subsystem Seams Only

When Linux-owner code needs host mediation, use narrow, subsystem-owned, private seams under `internal/ios/**`.

Allowed seam shape:
- specific to one subsystem
- minimal exported surface
- no ambient host vocabulary leakage

Forbidden seam shape:
- generic helper bags
- catch-all mediation headers used by unrelated subsystems
- abstractions that rename/deodorize host APIs and make them ambient

## 4) Ambient Host Vocabulary Is Forbidden in Linux-Owner Code

In `fs/`, `kernel/`, `runtime/`, `include/`, do not introduce:
- direct host APIs/types/macros
- renamed host APIs wrapped as generic helpers
- generic wrapper families for mutex/thread/cond/signal/io/platform bridging
- broad mediation headers that encode host assumptions globally

Category rule: banning one prefix and reintroducing the same leakage with a new prefix is still a violation.

## 5) Public ABI Discipline

- Public syscall names remain Linux-shaped.
- No branded/public ABI names that encode platform identity.
- Darwin/BSD header behavior must not define Linux-facing contracts.
- Keep internal implementation behind private `*_impl()` helpers and preserve clean public wrapper boundaries.

## 6) Proof Discipline (Required)

Lint green is necessary but insufficient.
Build green is necessary but insufficient.

Authoritative proof target is iOS Simulator:
1. `bash ./scripts/lint_linux_surface.sh`
2. `xcodegen generate --project .`
3. `xcodebuild build-for-testing -project IXLandSystem.xcodeproj -scheme IXLandSystem-6.12-arm64 -sdk iphonesimulator -configuration Debug -destination 'platform=iOS Simulator,name=iPhone 17'`
4. required targeted tests for the current tranche

Catalyst may be secondary smoke only.
No commit/push before required proof is green.

## 7) Tranche Discipline

Changes must be bounded by subsystem tranche with explicit ownership and proof.

Do not mix unrelated architecture migrations into one tranche.
Do not “fix lint” by weakening checks or broadening allowlists.

## 8) No Policy Theater

Forbidden:
- incident-specific blacklist hacks (single test/helper name grudges)
- fake completion claims without repo truth and proof logs
- cosmetic renames that preserve the same architectural violation

Required response to lint conflicts:
- refine seam boundaries
- relocate host mechanics behind `internal/ios/**`
- preserve Linux-owner semantics and contracts
