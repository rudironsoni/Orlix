# ADR 0022: Use Linux ELF With Orlix TCTI On iOS

## Status

Accepted, revised 2026-06-30.

## Context

Orlix has separate products that must not collapse into one runtime facade:

- `OrlixKernel` is upstream Linux plus the `arch/orlix` port, compiled into the iOS-hosted kernel product.
- `OrlixMLibC` is the libc for Orlix Linux userspace and tracks upstream mlibc.
- `OrlixOS` is the delivered OS Kit and app-facing Linux session and payload surface.
- `OrlixHostAdapter` owns private iOS/Darwin mediation only.

iOS and XNU/Darwin are the physical host environment. They are not the Orlix userspace ABI. Orlix userspace must see Linux UAPI and Linux syscall behavior owned by `OrlixKernel`.

Physical iPhone evidence invalidated the old initial backend assumption. Linux reached `/init`, then the native hosted execution path attempted to make copied anonymous Linux ELF text executable and iOS returned `KERN_PROTECTION_FAILURE`.

## Decision

The primary userspace package format remains ordinary AArch64 Linux ELF. Linux `execve()` and `binfmt_elf` remain the loading model. Linux VMAs, page tables, syscall numbers, errno behavior, VFS, fd tables, signals, wait/reaping, and process semantics remain owned by `OrlixKernel`.

OrlixMLibC remains the libc for Orlix-built Linux userspace. Normal unmodified AArch64 Linux binaries issuing `svc #0` are compatibility targets.

Direct host execution of guest Linux ELF is not a product-development or release backend. It may remain as a separate low-level oracle or benchmark, but it is not product evidence and it is not the TestFlight/App Store path.

This is a hybrid execution design, not an all-interpreted system. The Orlix app, OrlixKernel, OrlixHostAdapter, device and storage backends, terminal rendering, and signed app-native transports continue to execute natively. Linux syscalls, VFS, scheduling, signals, PTYs, containers, and process semantics execute natively inside OrlixKernel. TCTI is limited to ordinary guest AArch64 Linux EL0 instructions whose Linux ELF text cannot safely or legally become host-executable on App Store iPhone and iPad builds.

Product development and simulator validation use the same TCTI guest backend and executable-memory restrictions as the published app. Simulator-only host capabilities must not substitute for release-equivalent proof. A separate direct-native guest oracle may be used for comparison or diagnosis, but it cannot satisfy product gates. App Store guest ELF text must not use executable anonymous mappings, generated executable pages, JIT, MAP_JIT, or RWX memory.

The first physical-iPhone userspace backend is Orlix TCTI. TCTI belongs under `arch/orlix` and only owns guest AArch64 EL0 instruction fetch, decode, data-only gadget-program dispatch, guest register execution, guest memory fast paths, `svc #0` exits, user fault exits, yield/signal exits, unsupported-instruction reporting, and hot-path counters.

Guest ELF text pages remain host data mappings. Linux `VM_EXEC` remains meaningful, but TCTI enforces execute permission using Linux-owned VMA/PTE or Orlix arch/mm metadata. TCTI must not require JIT, MAP_JIT, RWX memory, generated executable memory, host executable page permissions for guest ELF text, Mach-O translated guest binaries, modified guest binaries, Wasm, or QEMU.

The execution stack is:

```text
Linux ELF / AArch64 Linux userspace
  -> Linux execve / binfmt_elf
  -> Linux VMAs / mm / task state
  -> arch/orlix hosted user-entry
  -> Orlix TCTI executes guest EL0 instructions
  -> guest svc #0 exits to arch/orlix syscall entry
  -> OrlixKernel dispatches Linux syscall semantics
  -> TCTI resumes the same Linux task
```

`OrlixHostAdapter` may provide host mechanics such as memory backing, console mirror, timers, lifecycle observation, device logging, resource registration, and host block/file/image backing for Linux drivers. It must not decode AArch64 guest instructions, decode Linux syscall numbers, own Linux process semantics, own Linux VFS/fd/signal/wait/exec behavior, implement Linux syscall policy, own guest page-table semantics, or know about TCTI gadget programs.

## Consequences

- Raw unmodified AArch64 Linux binaries issuing normal Linux `svc #0` are first-class compatibility targets.
- Orlix-built packages remain Linux ELF binaries linked against OrlixMLibC.
- No public Orlix syscall facade is added.
- TCTI must call the existing `arch/orlix` Linux syscall dispatch path rather than adding a syscall emulator.
- Guest text may be read by TCTI as data, but never mapped executable by the host.
- Host-native Orlix components and Linux kernel semantics do not pass through TCTI.
- Direct native guest execution may exist only as a separate oracle or benchmark and cannot satisfy product-development, simulator-readiness, or release gates.
- TCTI performance claims require exact workload, device or simulator, build configuration, command, baseline, counters, and Markdown report.
- Runtime proof must use boot progress, Linux console/PTY, HostAdapter console mirror, `linux-console` logs, and `host-vm` traces. UIKit screen state is not the proof surface.

## Rejected Alternatives

- Compiling packages as iOS Mach-O app code for the primary runtime format.
- Routing Linux userspace syscalls to Darwin `svc #0`.
- Adding a custom Orlix Linux-like userspace ABI.
- Embedding libc, shell, VFS, fd, process, or signal behavior in TCTI.
- Moving Linux syscall/process/VFS policy into OrlixHostAdapter.
- Treating QEMU, Wasm, JIT, MAP_JIT, RWX memory, generated executable memory, or host-executable guest text as the TestFlight/App Store path.
- Copying iSH, OpenMinis, or ios-linuxkit internals wholesale and renaming them Orlix.

Orlix TCTI is an Orlix-owned, arch/orlix, no-JIT, same-ISA, tail-call-threaded user-instruction backend for unmodified AArch64 Linux ELF binaries. It does not replace Linux; it lets OrlixKernel’s existing Linux userspace surface run on iOS without host-executable guest text.
