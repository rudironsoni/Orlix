# IXLandSystem: iOS Linux-Like Virtual Subsystem

<p align="center">
<img src="https://img.shields.io/badge/Platform-iOS%2016.0+-lightgrey.svg" alt="Platform: iOS">
<img src="https://img.shields.io/badge/arch-arm64%20device%20%7C%20arm64%2Fx86__64%20sim-blue.svg" alt="Architecture: iOS only">
</p>

**IXLandSystem** is a Linux-like virtual kernel subsystem for iOS, designed for maximum practical Linux userland compatibility within App Store constraints. It provides:

- Virtual `fork()`, `execve()`, and `waitpid()` without host process creation
- Unified task model for native commands and WAMR WASI modules
- PTY, job control, sessions, and process groups
- `/proc` and `/dev` virtual filesystems
- Complete file descriptor, signal, and VFS management
- Deterministic, trustworthy, and reproducible behavior

## Status

**Current Role**: This component (`IXLandSystem`) is the home of the main iXland implementation. Kernel, runtime, and syscall code lives here under canonical ownership:
- Filesystem: `IXLandSystem/fs/*`
- Kernel: `IXLandSystem/kernel/*`

## Platform Policy

- **Platform**: iOS only (iOS 16.0+)
- **Architectures**: arm64 device, arm64/x86_64 simulator
- **Build Target**: iphonesimulator and iphoneos SDKs only
- **Validation Authority**: iOS Simulator and Device tests only
- **macOS builds**: Not authoritative for correctness

## Architecture Overview

IXLand implements Linux userland semantics through a virtual kernel substrate:

```
┌─────────────────────────────────────────────────────────────┐
│                     iOS App Container                       │
├─────────────────────────────────────────────────────────────┤
│                   Linux Userland Apps                       │
│         (bash, coreutils, grep, sed, awk, etc.)            │
├─────────────────────────────────────────────────────────────┤
│                 POSIX Syscall Interface                     │
│               fork, execve, open, signal, etc.            │
├─────────────────────────────────────────────────────────────┤
│                    IXLand Virtual Kernel                       │
│  ┌─────────┬─────────┬─────────┬─────────┬─────────┐       │
│  │  Task   │ Signal  │  Exec   │   VFS   │  TTY    │       │
│  │Manager  │Handler  │Dispatch │ Filesys │Driver   │       │
│  └─────────┴─────────┴─────────┴─────────┴─────────┘       │
├─────────────────────────────────────────────────────────────┤
│              Native | WASI | Script Runtimes                │
│   ┌──────────┐   ┌──────────┐   ┌──────────┐              │
│   │  Native  │   │   WASI   │   │  Script  │              │
│   │ Commands │   │  (WAMR)  │   │ Shebang  │              │
│   └──────────┘   └──────────┘   └──────────┘              │
├─────────────────────────────────────────────────────────────┤
│                    iOS Host APIs                            │
└─────────────────────────────────────────────────────────────┘
```

### Key Objects

- **`ixland_task`**: Single canonical execution object with PID, PGID, SID
- **`ixland_files`**: Per-task file descriptor table
- **`ixland_fs`**: Per-task filesystem context (cwd, root, mounts)
- **`ixland_sighand`**: Per-task signal handlers
- **`ixland_tty`**: Controlling terminal and foreground pgrp
- **`ixland_exec_image`**: Currently executing image (native/WASI/script)

## Build System

**Single Source of Truth**: Xcode projects only. No CMake, no Make, no CTest.

The IXLand app project at `IXLand/IXLand.xcodeproj` includes IXLandSystem as a build dependency.

## Repository Layout

```
IXLandSystem/
├── fs/                    # Filesystem - canonical syscall ownership
│   ├── fdtable.c
│   ├── open.c
│   ├── read_write.c
│   ├── stat.c
│   ├── fcntl.c
│   ├── ioctl.c
│   ├── namei.c
│   ├── readdir.c
│   ├── select.c
│   ├── eventpoll.c
│   ├── exec.c
│   ├── path.c
│   ├── mount.c
│   ├── inode.c
│   └── super.c
├── kernel/                # Kernel subsystems - canonical ownership
│   ├── task.c
│   ├── fork.c
│   ├── exit.c
│   ├── wait.c
│   ├── pid.c
│   ├── cred.c
│   ├── sys.c
│   ├── signal.c
│   ├── time.c
│   ├── resource.c
│   ├── random.c
│   ├── sync.c
│   ├── init.c
│   ├── libc_delegate.c
│   └── net/
│       └── network.c
├── include/               # Public headers
├── Tests/                 # Test suite
│   ├── unit/              # Core unit tests
│   ├── iOS/               # iOS-specific tests
│   ├── harness/           # Test harness
│   └── fixtures/          # Test fixtures
└── docs/                  # Component documentation
```

## Wasm Boundaries

WAMR is the current WebAssembly runtime backend. Future abstraction:

- `IXLandWasm/IXLandWasmEngine/` will hold the engine-neutral contract
- `IXLandWasm/IXLandWasmHost/` will define host-service boundaries
- `IXLandWasm/IXLandWasmWASI/` will define WASI guest policy

For now, WAMR integration is handled within IXLandSystem.

## Native Command Registry

Native commands are pre-registered, not dynamically loaded:

```c
// Register a native command
IXLAND_NATIVE_CMD("/bin/ls", ixland_ls_main);
IXLAND_NATIVE_CMD("/bin/cat", ixland_cat_main);

// Entry ABI
int ixland_ls_main(ixland_task_t *task, int argc, char **argv, char **envp) {
    // Implementation
    return 0;
}
```

Registry is generated at build time from `runtime/native/commands/`.

## WASI/WAMR Integration

WAMR is an execution backend, not a separate universe:

- WASI operations use same `task->files` as native code
- WASI paths resolve through same VFS
- WASI stdio uses same descriptors
- WASI clocks/random through IXLand kernel abstractions

```c
// Load and run WASM module
ixland_wamr_load_module(wasm_buffer, wasm_size);
ixland_wamr_call_function("_start", argc, argv);
```

## Testing Doctrine

Every subsystem requires automated tests:

1. **Unit tests**: Data structures, deterministic logic
2. **Integration tests**: Cross-subsystem interactions
3. **Compatibility tests**: Linux userland behavior
4. **WASI tests**: WASI syscall compliance
5. **Device tests**: iOS-specific behavior

A feature is **not** complete without executable evidence.

## Constraints

- No real `fork()` or `execve()` image replacement
- No setuid
- No sandbox bypass
- No JIT or dynamic code generation
- No executable writable memory
- All WASI crossings validated

## Documentation

- `docs/IXLAND_ARCHITECTURAL_ANALYSIS.md` - Architecture details
- `docs/ARCHITECTURE.md` - Monorepo-level architecture
- `docs/BOUNDARIES.md` - Component boundary definitions
- `AGENTS.md` - Developer guidelines
- `docs/SYSCALLS.md` - Syscall reference
- `docs/PORTING.md` - Porting guide
- `Tests/README.md` - Testing guide

## License

[License information]
