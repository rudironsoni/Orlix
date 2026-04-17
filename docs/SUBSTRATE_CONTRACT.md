# IXLandSystem Substrate Contract

## Scope

IXLandSystem is the Linux-shaped runtime substrate for IXLand on iOS.
This repository owns virtual kernel/runtime behavior inside one iOS app sandbox.
It is not public drop-in proof for arbitrary Linux userspace yet.

## Platform

- host platform: iOS only
- minimum deployment target: iOS 16.0+
- supported SDKs: `iphonesimulator`, `iphoneos`
- authoritative validation: iOS Simulator or device build/test through XcodeGen + Xcodebuild

## Architectural Contract

Priority order:

1. Linux-shaped exported contract
2. Correct subsystem ownership
3. Xcode project and XcodeGen as build truth
4. Internalized iOS mediation
5. Deterministic subsystem behavior
6. Fewer downstream compatibility patches
7. Local implementation convenience

Decision rule:

If a change makes IXLandSystem less suitable as a Linux-oriented syscall, header, or runtime target, it is the wrong change.

## Linux ABI Truth

Vendored Linux 6.12 arm64 exported UAPI is the only Linux ABI truth in this repo.

Location:

- `third_party/linux-uapi/6.12/arm64/include`

Allowed vendored include forms:

- `#include <linux/...>`
- `#include <asm/...>`
- `#include <asm-generic/...>`

Forbidden vendored include forms:

- includes containing `third_party/linux-uapi`
- includes containing `6.12`
- includes containing `arm64`
- `../` traversal into vendored headers

Private internal headers are allowed only for private subsystem state, owner declarations, helper prototypes, and host-bridge declarations.
They must not define Linux ABI by hand.

## Build Truth

XcodeGen and the generated Xcode project are the only build truth.

Canonical project surface:

- Targets:
  - `IXLandSystem`
  - `IXLandSystemTests`
- Scheme:
  - `IXLandSystem-6.12-arm64`

Canonical authoritative flow:

```bash
xcodegen generate --project .
xcodebuild -list -project IXLandSystem.xcodeproj
xcodebuild -project IXLandSystem.xcodeproj -scheme IXLandSystem-6.12-arm64 -sdk iphonesimulator -arch arm64 -configuration Debug build
xcodebuild test -project IXLandSystem.xcodeproj -scheme IXLandSystem-6.12-arm64 -sdk iphonesimulator -configuration Debug -destination 'platform=iOS Simulator,name=iPhone 17'
```

`swift build`, CMake, Make, package manifests, and other build systems are non-authoritative drift for this repo.

## Darwin Quarantine

Darwin and iOS headers are private host implementation details.

Rules:

- Darwin host headers may appear only in private host-bridge files
- Darwin host headers must not define the Linux-facing contract
- public Linux-facing ownership must live in canonical owner files, not in Darwin bridge files

## Canonical Ownership

Current subsystem ownership in this repository:

- process/task lifecycle: `kernel/task.c`, `kernel/fork.c`, `kernel/exit.c`, `kernel/wait.c`, `kernel/pid.c`
- credentials: `kernel/cred.c`, `kernel/cred_internal.h`
- signals: `kernel/signal.c`, `kernel/signal.h`
- time and sync: `kernel/time.c`, `kernel/time_darwin.c`, `kernel/sync.c`
- init/sys/resource/random: `kernel/init.c`, `kernel/sys.c`, `kernel/resource.c`, `kernel/random.c`
- networking owner surface: `kernel/net/network.c`
- VFS and fdtable: `fs/vfs.c`, `fs/vfs.h`, `fs/fdtable.c`, `fs/fdtable.h`
- file operation owners: `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `fs/fcntl.c`, `fs/ioctl.c`, `fs/namei.c`, `fs/readdir.c`, `fs/eventpoll.c`, `fs/mount.c`, `fs/inode.c`, `fs/super.c`, `fs/path.c`, `fs/exec.c`
- native runtime registry: `runtime/native/registry.c`, `runtime/native/registry.h`
- private Darwin bridge surface: `arch/darwin/signal_bridge.c`

## Test Layering

This repo currently contains two valid proof layers in XCTest:

1. INTERNAL RUNTIME SEMANTIC TEST
   - may use private internal headers
   - may use `_impl()` entry points
   - may use direct internal owner APIs
   - does not prove public drop-in compatibility

2. LINUX UAPI / ABI COMPILE TEST
   - may use only `<linux/...>`, `<asm/...>`, `<asm-generic/...>` includes
   - proves vendored UAPI resolution only
   - does not prove runtime behavior

Current test files:

- `IXLandSystemTests/SignalTests.m` — INTERNAL RUNTIME SEMANTIC TEST
- `IXLandSystemTests/TaskGroupTests.m` — INTERNAL RUNTIME SEMANTIC TEST
- `IXLandSystemTests/CredentialTests.m` — INTERNAL RUNTIME SEMANTIC TEST
- `IXLandSystemTests/LinuxUAPICompileSmoke.c` — LINUX UAPI / ABI COMPILE TEST

True public drop-in Linux userspace compatibility proof is outside this XCTest tranche.

## Current Truth Boundaries

What this repo can currently prove authoritatively:

- canonical project generation through `xcodegen`
- canonical iOS build/test execution through `xcodebuild`
- Linux UAPI header resolution through canonical include paths
- selected runtime semantics for task groups, signals, and credentials via internal tests

What this repo does not currently prove:

- arbitrary unmodified Linux userspace compiling and linking against a stable public surface
- finished Linux compatibility across all subsystems
- a production-ready, fully complete runtime

## Documentation Rule

Documents in normal repo paths must describe current repo truth.
Historical migration plans and stale status documents should not remain as misleading authoritative documentation.
