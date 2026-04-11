# ixland: iOS Linux-Like Virtual Subsystem

<p align="center">
<img src="https://img.shields.io/badge/Platform-iOS%2016.0+-lightgrey.svg" alt="Platform: iOS">
<img src="https://img.shields.io/badge/arch-arm64%20device%20%7C%20arm64%2Fx86__64%20sim-blue.svg" alt="Architecture: iOS only">
</p>

**ixland** is a Linux-like virtual kernel subsystem for iOS, designed for maximum practical Linux userland compatibility within App Store constraints. It provides:

- Virtual `fork()`, `execve()`, and `waitpid()` without host process creation
- Unified task model for native commands and WAMR WASI modules
- PTY, job control, sessions, and process groups
- `/proc` and `/dev` virtual filesystems
- Complete file descriptor, signal, and VFS management
- Deterministic, trustworthy, and reproducible behavior

## Status

⚠️ **Work In Progress**: Transforming from compatibility layer to Linux-like subsystem. See `docs/IXLAND_ARCHITECTURAL_ANALYSIS.md` for details.

**Current Role**: This component (`ixland-system`) is the current home of the main iXland implementation. Most kernel, runtime, and syscall code lives here. Future narrow extractions will move public headers to `ixland-libc` and Wasm interfaces to `ixland-wasm`, but those boundaries are not yet populated.

## Platform Policy

- **Platform**: iOS only (iOS 16.0+)
- **Architectures**: arm64 device, arm64/x86_64 simulator
- **Build Target**: iphonesimulator and iphoneos SDKs only
- **Validation Authority**: iOS Simulator and Device tests only
- **macOS builds**: Not authoritative for correctness

## Architecture Overview

ixland implements Linux userland semantics through a virtual kernel substrate:

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
│                    ixland Virtual Kernel                       │
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

## Compatibility Target

Maximum practical Linux userland compatibility, including:

- bash (with job control)
- coreutils (ls, cp, mv, rm, cat, etc.)
- grep, sed, awk
- tar, xz, make
- findutils, readline, ncurses
- WASI workloads via WAMR

Not literal Linux kernel compatibility—rather, Linux-like userland behavior through virtual kernel substrate.

## Build System

**Single Source of Truth**: Xcode projects with supporting Makefiles

### Build Commands

```bash
# Build for iOS Simulator
make sdk-sim

# Build for iOS Device
make sdk-device

# Build all
make sdk
```

### Test Commands

```bash
# Build test app for simulator
cd libixlandTest/libixlandTest
xcodebuild -scheme libixlandTest -sdk iphonesimulator
```

## Repository Layout

```
ixland-system/
├── include/
│   ├── ixland/               # Public headers
│   │   ├── ixland.h
│   │   ├── ixland_syscalls.h
│   │   └── ixland_wamr.h
│   └── uapi/linux/        # Linux-compatible UAPI
├── kernel/                # Kernel subsystems
│   ├── task/              # fork, exit, wait, PID
│   ├── signal/            # signal delivery, pgrp, session
│   ├── exec/              # exec dispatch
│   ├── time/              # clocks
│   └── resource/          # rlimits
├── fs/                    # Filesystem
│   ├── vfs/               # VFS and mount namespaces
│   ├── proc/              # /proc filesystem
│   ├── dev/               # /dev filesystem
│   ├── devpts/            # /dev/pts
│   └── pipe/              # pipe implementation
├── drivers/
│   └── tty/               # PTY, termios, TTY
├── runtime/               # Execution backends
│   ├── native/            # Native command registry
│   ├── wasi/              # WAMR WASI bridge
│   └── script/            # Shebang interpreter
├── compat/
│   ├── posix/             # POSIX compatibility
│   └── interpose/         # Symbol interposition
├── tests/                 # Test suite
│   ├── unit/              # Core unit tests
│   ├── integration/       # Subsystem integration
│   ├── compat/            # Linux compatibility
│   ├── wasi/              # WASI tests
│   ├── device/            # Device-specific
│   ├── simulator/         # Simulator-specific
│   ├── stress/            # Stress tests
│   └── perf/              # Performance tests
└── tools/                 # Build and test scripts
```

## Wasm Boundaries

WAMR is the current WebAssembly runtime backend. Future abstraction:

- `ixland-wasm-engine/` will hold the engine-neutral contract
- `ixland-wasm-host/` will define host-service boundaries
- `ixland-wasm-wasi/` will define WASI guest policy

For now, WAMR integration is handled within this component.

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
- WASI clocks/random through ixland kernel abstractions

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
6. **Stress tests**: Concurrency and reliability
7. **Performance tests**: Regression baselines

A feature is **not** complete without executable evidence.

### Test Commands

```bash
# Build and run tests
make sdk-sim
cd libixlandTest/libixlandTest
xcodebuild test -scheme libixlandTest -sdk iphonesimulator -destination "platform=iOS Simulator,name=iPhone 17 Pro"
```

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
- `tests/README.md` - Testing guide

## Development Status

- **Phase 0**: Repository reorganization (complete)
- **Phase 1**: Core kernel objects (task, files, fs, signal)
- **Phase 2**: Fork/exec implementation
- **Phase 3**: PTY, job control, signals
- **Phase 4**: Native command registry
- **Phase 5**: WASI integration
- **Phase 6**: Test infrastructure
- **Phase 7**: Bash compatibility validation

See `docs/IXLAND_ARCHITECTURAL_ANALYSIS.md` for detailed implementation order.

## License

[License information]
