# libixland Architecture Specification

## Overview

**libixland** is the iOS eXtension subsystem - a complete Linux syscall compatibility layer for iOS. It enables unmodified Linux binaries to run on iOS through static symbol interposition and optional WebAssembly support.

## Naming Convention

| Level | Pattern | Example |
|-------|---------|---------|
| Internal Implementation | `__ixland_*_impl()` | `__ixland_fork_impl()` |
| Public API | `ixland_*()` | `ixland_fork()` |
| Linux Standard | `#define syscall ixland_syscall` | `#define fork ixland_fork` |
| Tools | `ixland-*` | `ixland-cc`, `ixland-pkg` |

## Directory Structure

```
a-shell-kernel/
├── include/ixland/              # Public headers
│   ├── ixland.h                 # Master umbrella header
│   ├── ixland_syscalls.h        # Syscall prototypes (public)
│   ├── ixland_types.h           # ixland-specific types
│   ├── ixland_error.h           # Error codes
│   ├── ixland_wamr.h            # WAMR integration API
│   └── sys/                  # Linux-compatible system headers
│       ├── unistd.h          # fork, exec, getpid...
│       ├── wait.h            # waitpid...
│       ├── time.h            # time functions
│       ├── stat.h            # stat structures
│       ├── types.h           # pid_t, off_t...
│       ├── socket.h          # BSD sockets
│       ├── signal.h          # signal handling
│       ├── mman.h            # mmap, mprotect
│       ├── fcntl.h           # open flags
│       ├── ioctl.h           # ioctl
│       └── errno.h           # errno codes
│
├── src/ixland/                  # ALL implementations
│   ├── core/                 # Core syscall implementations
│   │   ├── ixland_process.c     # fork, exec, wait, pid
│   │   ├── ixland_file.c        # open, read, write, close
│   │   ├── ixland_signal.c      # signal handling
│   │   ├── ixland_memory.c      # mmap, munmap, mprotect
│   │   ├── ixland_time.c        # sleep, gettimeofday
│   │   ├── ixland_network.c     # socket passthrough
│   │   └── ixland_thread.c      # pthread utilities
│   │
│   ├── interpose/            # Symbol interposition layer
│   │   └── ixland_interpose.c   # All 300+ syscall wrappers
│   │
│   ├── runtime/              # C runtime (from libc_replacement)
│   │   ├── ixland_stdio.c       # printf, fprintf, etc.
│   │   ├── ixland_stdlib.c      # malloc, free wrappers
│   │   ├── ixland_env.c         # getenv, setenv
│   │   ├── ixland_string.c      # string functions
│   │   └── ixland_crt0.c        # C startup code
│   │
│   ├── wamr/                 # WAMR integration
│   │   ├── ixland_wamr_runtime.c     # WAMR initialization
│   │   ├── ixland_wamr_wasi.c        # WASI syscall bridge
│   │   ├── ixland_wamr_aot.c         # AoT loader
│   │   └── ixland_wamr_native.c      # Native symbol exports
│   │
│   ├── util/                 # Utilities
│   │   ├── ixland_vfs.c         # Virtual filesystem
│   │   ├── ixland_path.c        # Path utilities
│   │   └── ixland_debug.c       # Debug/trace functions
│   │
│   └── internal/             # Private headers
│       └── ixland_internal.h      # Internal data structures
│
├── lib/                      # Build outputs (gitignore)
│   ├── libixland.a
│   └── libixland.dylib
│
├── bin/                      # Tools (generated)
│   └── ixland-cc                # Compiler wrapper
│
├── deps/                     # Dependencies
│   └── wamr/                 # WAMR submodule
│
├── tests/                    # Test suite
│   ├── unit/
│   ├── integration/
│   └── fixtures/
│
├── CMakeLists.txt            # Primary build system
└── Makefile                  # Convenience wrapper
```

## Architecture Layers

### Layer 1: Symbol Interposition (Strong Symbols)

Linux programs call standard syscalls (fork, exec, open) which are intercepted at link time:

```c
// libixland.a provides these as strong symbols:
pid_t fork(void) { return ixland_fork(); }
int execve(...) { return ixland_execve(...); }
int open(...) { return ixland_open(...); }
```

**Compile command:**
```bash
ixland-cc program.c -o program_ios
# Internally: -I/path/to/ixland/include -lixland
```

### Layer 2: Public API (ixland_*)

Clean public interface, no macro pollution:

```c
pid_t ixland_fork(void);
int ixland_execve(const char *pathname, char *const argv[], char *const envp[]);
int ixland_open(const char *pathname, int flags, mode_t mode);
```

### Layer 3: Internal Implementation (__ixland_*_impl)

Actual implementation with iOS-safe code:

```c
pid_t __ixland_fork_impl(void) {
    // Thread-based process simulation
    // Virtual PID allocation
    // iOS-safe implementation
}
```

## Dual Execution Model

### Track A: Native (ixland-compiled)

```
Linux Source ──► ixland-cc ──► libixland.a ──► Native ARM64 Binary
                (compile)   (syscalls)    (100% performance)
```

### Track B: WASM (WAMR AoT)

```
WASM Binary ──► ixland-wamr ──► WAMR AoT ──► 90-95% performance
                (runner)    (runtime)
```

## Syscall Coverage

### Process Management (16 syscalls)
- fork, vfork, execve, execv, exit, _exit
- getpid, getppid, getpgrp, setpgrp
- getpgid, setpgid, wait, waitpid, wait3, wait4
- system

### File Operations (20 syscalls)
- open, openat, creat, read, write, close
- lseek, pread, pwrite, dup, dup2, dup3
- fcntl, ioctl, access, faccessat
- chdir, fchdir, getcwd

### Filesystem (24 syscalls)
- stat, fstat, lstat, fstatat
- mkdir, mkdirat, rmdir, unlink, unlinkat
- link, linkat, symlink, symlinkat
- readlink, readlinkat
- chmod, fchmod, fchmodat
- chown, fchown, lchown, fchownat
- chroot

### Signal Handling (16 syscalls)
- signal, kill, killpg, raise, sigaction
- sigprocmask, sigpending, sigsuspend
- sigemptyset, sigfillset, sigaddset, sigdelset, sigismember
- alarm, setitimer, getitimer, pause

### Memory Management (6 syscalls)
- mmap, munmap, mprotect, msync, mlock, munlock

### Time (7 syscalls)
- sleep, usleep, nanosleep, gettimeofday, settimeofday
- clock_gettime, time

### Environment (5 syscalls)
- getenv, setenv, unsetenv, clearenv, environ

### Network (20 syscalls - passthrough)
- socket, socketpair, bind, connect, listen, accept, accept4
- send, sendto, sendmsg, recv, recvfrom, recvmsg
- shutdown, getsockname, getpeername
- getsockopt, setsockopt

### Pipes (4 syscalls)
- pipe, pipe2, mkfifo, mkfifoat

### Select/Poll (4 syscalls)
- select, pselect, poll, ppoll

### TTY (7 syscalls)
- isatty, ttyname_r, tcgetattr, tcsetattr
- tcsendbreak, tcdrain, tcflush, tcflow

## WAMR Integration

### WASI Syscalls (100+)

WAMR runs WebAssembly binaries with WASI syscalls bridged to ixland:

```c
static NativeSymbol ixland_wasi_symbols[] = {
    {"fd_write", ixland_wasi_fd_write, "(i****)"},
    {"fd_read", ixland_wasi_fd_read, "(i****)"},
    {"proc_exit", ixland_wasi_exit, "(i)"},
    // ... 100+ more
};
```

### Performance

| Mode | Speed | Use Case |
|------|-------|----------|
| Native (ixland) | 100% | Primary - recompiled tools |
| WAMR AoT | 90-95% | Fallback - prebuilt WASM |
| WAMR Interpreter | 10-20% | Debug only |

## Build System

### CMake

```cmake
add_library(ixland STATIC
    src/ixland/core/*.c
    src/ixland/interpose/*.c
    src/ixland/runtime/*.c
    src/ixland/wamr/*.c
)

target_link_libraries(ixland PUBLIC
    pthread
    ${WAMR_LIBS}
)
```

### Compiler Wrapper (ixland-cc)

```bash
#!/bin/bash
clang \
    -target arm64-apple-ios16.0 \
    -isystem /path/to/ixland/include \
    -L/path/to/ixland/lib -lixland \
    "$@"
```

## Migration from a-shell-kernel

### Old to New Mapping

| Old | New |
|-----|-----|
| `a_shell_fork()` | `__ixland_fork_impl()` (internal) |
| | `ixland_fork()` (public) |
| `a_shell_system.m` | `src/ixland/core/ixland_process.c` |
| `libc_replacement.c` | `src/ixland/runtime/ixland_*.c` |
| `include/linux/unistd.h` | `include/ixland/sys/unistd.h` |
| `#define fork a_shell_fork` | `pid_t fork(void) { return ixland_fork(); }` |

### Migration Script

```bash
./scripts/migrate_big_bang.sh
```

This script:
1. Creates new directory structure
2. Moves old files to backup locations
3. Deletes old directories
4. Leaves manual refactoring to developer

## Security Considerations

- **iOS Sandbox**: All syscalls respect app sandbox boundaries
- **No JIT**: WAMR uses AoT (ahead-of-time) compilation - App Store compliant
- **Path Validation**: All paths validated before access
- **No fork()**: Thread-based process simulation
- **W^X Policy**: No executable memory modifications

## App Store Compliance

✅ **Static Library**: libixland.a - no dynamic code generation
✅ **AoT Compilation**: WASM → ARM64 at build time
✅ **Sandboxed**: All file/network access within iOS sandbox
✅ **No Private APIs**: Only public iOS APIs used

## Future Work

- CHROOT support for namespace isolation
- Network sandboxing compliance
- Performance benchmarks
- Package manager (ixland-pkg)
- CI/CD for automatic package building

---

**Version**: 1.0.0  
**Last Updated**: 2025-01-XX  
**Status**: Foundation Complete, Implementation In Progress
