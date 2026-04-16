# IXLandSystem Substrate Contract

## Scope

`IXLandSystem` is the Linux-shaped substrate for iOS host execution.
This repository is not the app, not libc, not packaging, and not wasm planning.

## Platform

- Host platform: iOS only
- Minimum deployment: iOS 16.0+
- Supported SDKs: `iphonesimulator`, `iphoneos`
- iOS Simulator and Device tests are the only validation authority

## Canonical Contract

- Canonical behavior is Linux-shaped
- Names, constants, flags, structs, errno, fd semantics, signals, termios, ioctl, poll, path, process behavior MUST prefer Linux expectations unless impossible under iOS constraints
- iOS mediation MUST stay private implementation detail
- Host behavior MUST NOT redefine canonical subsystem truth
- Minimize product-prefixed naming in exported ABI and UAPI when Linux-shaped naming is viable

## Naming Debt

The current public surface uses `ixland_*` prefixes on Linux syscall names:

- `ixland_fork`, `ixland_execve`, `ixland_waitpid`, `ixland_setpgid`, etc.

This violates the project contract. Linux syscall-facing names SHOULD be Linux-shaped (`fork`, `execve`, `waitpid`, `setpgid`) in the exported surface.

The `ixland_` prefix is acceptable ONLY for:
- Internal infrastructure with no Linux analog (`ixland_task_t`, `ixland_files_t`)
- Private subsystem bookkeeping
- IXLand-specific initialization/versioning (`ixland_init`, `ixland_version`)

## Public Surface Boundaries

Current public headers:

- `include/ixland/ixland_syscalls.h` — process/exec/init syscall seam
- `include/ixland/ixland_signal.h` — signal-mask type and operations (DEBT: ixland_ prefixed)
- `include/ixland/ixland_path.h` — path classification/translation
- `include/ixland/ixland_types.h` — shared public types
- `include/vfs.h` — legacy VFS header (DIVERGES from owner `fs/vfs.h`)

Owner internal headers:

- `kernel/task.h`, `kernel/signal.h`
- `fs/fdtable.h`, `fs/vfs.h`, `fs/path.h`, `fs/tty/tty.h`

## Canonical Ownership

- Process lifecycle and identity: `kernel/*`
- Signal ownership/delivery: `kernel/signal.c`, `kernel/signal.h`
- File descriptor table: `fs/fdtable.c`, `fs/fdtable.h`
- VFS/path/stat syscall ownership: `fs/*`
- Time and sync syscall ownership: `kernel/time.c`, `kernel/sync.c`
- Exec syscall ownership: `fs/exec.c`, `fs/exec.h`
- Init/version: `kernel/init.c`

No new branded public header namespace is introduced.

## Explicit Gaps

These are currently incomplete:

- VFS core path walk and lookup (`vfs_lookup`, `vfs_path_walk`) — stubs returning `-ENOSYS`
- VFS stat/access (`vfs_stat_path`, `vfs_lstat`, `vfs_access`) — stubs returning `-ENOSYS`
- Futex (`ixland_futex`) — returns `-ENOSYS`
- TTY/PTY implementation file absent; `fs/tty/tty.h` declares API with no `.c`
- `mmap`/`munmap`/`mprotect` — not implemented
- `openat`, `pipe` — not implemented
- 8 signal syscall public functions (`sigaction`, `kill`, `sigprocmask`, etc.) — not implemented
- 11 signal mask API functions declared in `ixland_signal.h` — no implementation
- `include/vfs.h` diverges from owner `fs/vfs.h`
- `kernel/pid.c` exports `ixland_alloc_pid`/`ixland_free_pid`/`ixland_pid_init` but `kernel/task.h` declares `alloc_pid`/`free_pid`/`pid_init` (symbol mismatch)

## Build Truth

- **Authoritative build path**: `xcodegen generate --project .` then `xcodebuild` with iOS SDK
- `project.yml` is the authoritative project spec for XcodeGen
- `xcodebuild -project IXLandSystem.xcodeproj -scheme IXLandSystem-6.12-arm64 -sdk iphonesimulator -arch arm64 -configuration Debug build` produces the iOS Simulator build artifacts
- `xcodebuild test -project IXLandSystem.xcodeproj -scheme IXLandSystem-6.12-arm64 -sdk iphonesimulator -configuration Debug -destination 'platform=iOS Simulator,name=iPhone 17'` is the canonical executable test invocation
- `.github/workflows/build.yml` runs XcodeGen + xcodebuild as CI gate
- `swift build` is non-authoritative drift and must not be used as iOS build proof

## Mach-O Export Spike Status

**Status: INCONCLUSIVE**

The `__attribute__((weak, alias("target")))` pattern was not tested in isolation.
The claim that "Darwin does not support weak symbols/aliases" was overstated.
Clang documents both `weak` and `alias` as supported attributes.
Whether the specific combination works for iOS/Mach-O requires isolated testing.

## Rename Preconditions

Before any mass rename:

1. **Function name collision:** `stat()`, `open()` etc. conflict with standard library declarations
2. **Recursion risk:** Internal implementations must not call themselves
3. **Declaration drift:** Headers must match implementations
4. **Link-time issues:** Export mechanism must be proven viable
5. **ENOSYS stubs:** Incomplete implementations become more visible with standard names

## Last Updated

2026-04-14
