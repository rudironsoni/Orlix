# Agent Guide: IXLandSystem

## Overview

**IXLandSystem** is a Linux-like virtual kernel subsystem for iOS, designed for maximum practical Linux userland compatibility within App Store constraints. It is not a compatibility shim—it is a virtual kernel substrate that implements Linux process, filesystem, signal, and terminal semantics.

## Mission Statement

Transform the project into one coherent Linux-like virtual subsystem for iOS, achieving:

1. Maximum practical Linux userland compatibility (bash, coreutils, etc.)
2. Zero reliance on real host `fork()` or image replacement
3. One shared substrate for native commands and WAMR WASI modules
4. Deterministic structure mirroring Linux subsystem boundaries
5. Deterministic, trustworthy, reproducible build and test behavior

## Platform Policy (Non-negotiable)

- **Platform**: iOS only
- **Minimum deployment**: iOS 16.0+
- **Supported SDKs**: `iphonesimulator` and `iphoneos` only
- **Validation authority**: iOS Simulator and Device tests only
- **macOS builds**: Not authoritative for correctness
- **Simulator**: iPhone 17 Pro only (no other simulator types allowed)

**Hard constraint**: Configure must hard-fail if not targeting iOS.

## Build System

**Xcode-only. No CMake. No Make. No CTest.**

The build truth is in `IXLand/IXLand.xcodeproj`. IXLandSystem is built as part of the app project.

## Architectural Principles

### Single Canonical Execution Object

There is exactly one canonical execution object: **`ixland_task`**.

- All FDs live only under `task->files`
- No global state for process concepts

### WAMR as Execution Backend

WAMR is a secondary execution backend, not a separate universe:

- WASI uses same `task->files`, `task->fs`, `task->sighand`
- WAMR does not define independent process, fs, or signal models
- App customers access WAMR only through `ixland_wamr_*`
- `deps/wamr/` remains pristine (read-only upstream)

### Virtual Process Model

No reliance on host `fork()` or `execve()`:

- `fork()`: Virtual process creation inside IXLand task system
- `execve()`: Virtual image replacement (native/WASI/script)
- Parent and child are separate `ixland_task` objects
- All semantics implemented by IXLand, not host kernel

### Native Command Registry

Native commands execute through a registry, not arbitrary loading:

- Pre-registered at build time
- Entry ABI: `int ixland_native_main(ixland_task_t *task, int argc, char **argv, char **envp)`
- Examples: `/bin/bash`, `/bin/ls`, `/usr/bin/env`

## Core Objects

### ixland_task

The single canonical task structure:

```c
typedef struct ixland_task {
    pid_t pid, ppid, tgid, pgid, sid;
    atomic_int state;
    int exit_status;
    pthread_t thread;

    ixland_files_t *files;     /* FD table (per-task) */
    ixland_fs_t *fs;           /* Filesystem context */
    ixland_sighand_t *sighand; /* Signal handlers */
    ixland_tty_t *tty;         /* Controlling terminal */
    ixland_exec_image_t *exec_image; /* Current executable */

    struct ixland_task *parent, *children, *next_sibling;
} ixland_task_t;
```

### Ownership Rules

| Concept | Owner | Location |
|---------|-------|----------|
| PID | ixland_task | `task->pid` |
| FDs | ixland_files | `task->files->fd[]` |
| CWD | ixland_fs | `task->fs->cwd` |
| Signal handlers | ixland_sighand | `task->sighand->action[]` |
| TTY | ixland_tty | `task->tty` |
| Mounts | ixland_mount_ns | `task->fs->mnt_ns` |

**No global state** for these concepts.

## Repository Layout

```
IXLandSystem/
├── kernel/              # Kernel subsystems - canonical ownership
│   ├── task.c           # Process management
│   ├── fork.c           # Fork implementation
│   ├── exit.c           # Exit handling
│   ├── wait.c           # Waitpid implementation
│   ├── pid.c            # PID allocation
│   ├── cred.c           # Credentials
│   ├── sys.c            # Syscall dispatch
│   ├── signal.c         # Signal delivery
│   ├── time.c           # Clocks
│   ├── resource.c       # rlimits
│   ├── random.c         # Random numbers
│   ├── sync.c           # Synchronization
│   ├── init.c           # Initialization
│   ├── libc_delegate.c  # LibC delegation
│   └── net/
│       └── network.c    # Network subsystem
├── fs/                  # Filesystem - canonical syscall ownership
│   ├── fdtable.c        # File descriptor table
│   ├── open.c           # Open syscall
│   ├── read_write.c     # Read/write syscalls
│   ├── stat.c           # Stat syscalls
│   ├── fcntl.c          # Fcntl syscall
│   ├── ioctl.c          # Ioctl syscall
│   ├── namei.c          # Path resolution
│   ├── readdir.c        # Directory operations
│   ├── select.c         # Select/poll
│   ├── eventpoll.c      # Epoll
│   ├── exec.c           # Exec implementation
│   ├── path.c           # Path operations
│   ├── mount.c          # Mount operations
│   ├── inode.c          # Inode cache
│   └── super.c          # Superblock operations
├── include/             # Public headers
├── Tests/               # Test suite
└── docs/                # Documentation
```

## Testing Doctrine

### Truthfulness Rule

A feature is **NOT** complete without:
1. Deterministic automated test, OR
2. Conformance test, OR
3. Regression test, OR
4. Documented limitation

### Required Test Layers

1. **Unit tests**: Data structures, logic
2. **Integration tests**: Cross-subsystem
3. **Compatibility tests**: Linux userland
4. **WASI tests**: WASI compliance
5. **iOS tests**: Simulator and device
6. **Stress tests**: Concurrency
7. **Performance tests**: Regression

### Platform Testing Policy

- **Simulator**: Structural, integration, most correctness
- **Device**: Final authority, PTY, sandbox, timing
- **macOS**: Not authoritative

## Naming Conventions

### Three-Level Naming

| Level | Pattern | Purpose | Example |
|-------|---------|---------|---------|
| Internal | `__ixland_*_impl()` | Implementation | `__ixland_fork_impl()` |
| Public | `ixland_*()` | Public API | `ixland_fork()` |
| Interposed | Standard names | Interposition | `fork()` |

### WAMR Naming

| Layer | Pattern | Purpose |
|-------|---------|---------|
| Public | `ixland_wamr_*` | App-facing API |
| Internal | `runtime/wasi/*` | Bridge logic |
| Upstream | `deps/wamr/*` | Read-only (no edits) |

## Agent Instructions

### Always Do

1. Keep `deps/wamr/` pristine—no edits
2. Implement Linux semantics in IXLand, not passthrough
3. Use `ixland_task` as single execution model
4. Put FDs only under `task->files`
5. Test every subsystem with automated tests
6. Require iOS Simulator/Device validation
7. Document customer-facing API limits
8. Commit regression tests with bug fixes
9. Use deterministic test commands
10. Pin simulator/device in test scripts

### Never Do

1. Do not edit `deps/wamr/` (unless explicit upstream patch request)
2. Do not rely on host `fork()` or `execve()`
3. Do not use global FD tables
4. Do not keep dual process models
5. Do not present WAMR as separate universe
6. Do not mark features complete without tests
7. Do not use macOS-only validation
8. Do not rely on manual testing
9. Do not weaken bash compatibility goal
10. Do not leave high-risk code without stress tests

## Compatibility Target

Maximum practical Linux userland compatibility:

- **Supported**: bash, coreutils, grep, sed, awk, tar, make, findutils, readline, ncurses
- **Required**: PTY, job control, signals, /proc, /dev, pipes, redirections
- **WASI**: Same substrate as native code

Not "just POSIX"—Linux-oriented userland compatibility like Termux/Cygwin/MSYS2.

## Security Constraints

- No real fork/exec
- No setuid
- No sandbox bypass
- No JIT or dynamic code generation
- No executable writable memory
- Validate all WASI crossings

## Documentation

- `README.md`: User-facing overview
- `docs/IXLAND_ARCHITECTURAL_ANALYSIS.md`: Architecture and migration
- `docs/SYSCALLS.md`: Syscall reference
- `docs/PORTING.md`: Porting guide
- `Tests/README.md`: Testing guide

## Last Updated

2026-04-11

**Primary Rule**: `deps/wamr/` is external, app customers access WAMR only through `ixland_wamr_*`, and every subsystem requires executable test evidence.
