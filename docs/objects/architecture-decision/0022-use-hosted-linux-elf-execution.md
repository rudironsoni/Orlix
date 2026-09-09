---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-09-09
status: accepted
external_id: "ADR-0022"
summary: "Durable Orlix architecture decision ADR 0022."
part_of:
  - "[Orlix](../product/orlix.md)"
derived_from:
  - "[TCTI reference review](../../sources/tcti/reference-review.md)"
amended_by:
  - "[ADR 0029](0029-separate-complete-aarch64-target-from-runtime-profile.md)"
  - "[ADR 0031](0031-keep-arm-shared-asl-external-and-prove-orlixtcti-independently.md)"
  - "[ADR 0039](0039-cache-tcti-basic-blocks-as-gadget-programs.md)"
---

# ADR 0022: Use Linux ELF With Orlix TCTI On iOS

## Status

Accepted, revised 2026-07-24.

## Context

Orlix has separate products that must not collapse into one runtime facade:

- `OrlixKernel` is upstream Linux plus the `arch/orlix` port, compiled into the iOS-hosted kernel product.
- `OrlixMLibC` is the libc for Orlix Linux userspace and tracks upstream mlibc.
- `OrlixOS.xcframework` is the sole public SDK and app-facing `OrlixMachine` and `OrlixOS.Containers` surface.
- `OrlixHostAdapter` owns private iOS/Darwin mediation only.

iOS and XNU/Darwin are the physical host environment. They are not the Orlix userspace ABI. Orlix userspace must see Linux UAPI and Linux syscall behavior owned by `OrlixKernel`.

Physical iPhone evidence invalidated the old initial backend assumption. Linux reached `/init`, then the native hosted execution path attempted to make copied anonymous Linux ELF text executable and iOS returned `KERN_PROTECTION_FAILURE`.

## Decision

The primary userspace package format remains ordinary AArch64 Linux ELF. Linux `execve()` and `binfmt_elf` remain the loading model. Linux VMAs, page tables, syscall numbers, errno behavior, VFS, fd tables, signals, wait/reaping, and process semantics remain owned by `OrlixKernel`.

OrlixMLibC remains the libc for Orlix-built Linux userspace. Normal unmodified AArch64 Linux binaries issuing `svc #0` are compatibility targets.

Direct host execution of guest Linux ELF is not a product-development or release backend. It may remain as a separate low-level oracle or benchmark, but it is not product evidence and it is not the TestFlight/App Store path.

This is a hybrid execution design, not an all-interpreted system. The Orlix app, OrlixKernel, OrlixHostAdapter, device and storage backends, terminal rendering, and signed app-native transports continue to execute natively. Linux syscalls, VFS, scheduling, signals, PTYs, containers, and process semantics execute natively inside OrlixKernel. TCTI is limited to ordinary guest AArch64 Linux EL0 instructions whose Linux ELF text cannot safely or legally become host-executable on App Store iPhone and iPad builds.

Product development and simulator validation use the same TCTI guest backend and executable-memory restrictions as the published app. Simulator-only host capabilities must not substitute for release-equivalent proof. A separate direct-native guest oracle may be used for comparison or diagnosis, but it cannot satisfy product gates. App Store guest ELF text must not use executable anonymous mappings, generated executable pages, JIT, MAP_JIT, or RWX memory.

The first physical-iPhone userspace backend is `OrlixTCTI`. OrlixTCTI belongs under `arch/orlix` and only owns guest AArch64 EL0 instruction fetch, decode, data-only gadget-program dispatch, guest register execution, guest memory fast paths, `svc #0` exits, user fault exits, yield/signal exits, unsupported-instruction reporting, and hot-path counters.

TCTI completion requires the complete AArch64 EL0 instruction set exposed to the guest, including integer, branch, load/store, atomic, SIMD, floating-point, crypto, and system-register behavior available in the declared guest ISA profile. Package workloads may prioritize implementation order, but they do not define instruction coverage. An architecturally valid instruction in the exposed profile may not be replaced by an unsupported-instruction exit, a hard-coded workload special case, or a reduced semantic approximation. Reserved, unallocated, privileged, and unadvertised optional-extension encodings must produce their architecturally defined exception or deterministic TCTI exit.

ADR 0029 refines only how completeness is counted and capabilities are
promoted. The complete pinned Arm source inventory remains visible as the
implementation target even while the runtime HWCAP profile advertises a smaller
proved subset. That refinement does not broaden TCTI beyond guest EL0
instruction execution or change any Linux, OrlixHostAdapter, executable-memory,
or App Store ownership boundary in this decision.

TCTI has one authoritative ISA source hierarchy. The pinned Arm AARCHMRS
2026-06 release defines the complete 4,350-leaf target, including feature
predicates, aliases, duplicates, encoding relationships, and non-EL0 or
undefined behavior. The separately pinned official Arm shared-ASL corpus
defines instruction semantics. Runtime HWCAP and HWCAP2 are a downstream
advertisement projection and never define, filter, or reduce the completion
target.

Linux arm64 sources, Linux kselftests, and KVM selftests may inform Linux
integration and test selection. Islaris, Isla, and research describing
validation against authoritative Arm specifications may inform verification
methodology. None of those sources supplies TCTI instruction semantics. QEMU,
Unicorn, Sail-generated code, external interpreters, external instruction
generators, host-side ISA models, terminal output, and log scraping cannot act
as semantic or proof authorities.

Every AARCHMRS leaf must resolve through a machine-checkable, C-native
source-to-proof record under `arch/orlix`. At minimum, the record retains its
AARCHMRS identifier, instruction family, feature predicates, EL and semantic
classification, alias or duplicate relationship, owning production
implementation, owning KUnit, applicable Linux kselftest, runtime HWCAP or
HWCAP2 dependency, implementation status, and proof status. The build fails
when any required source, semantic provenance, classification, ownership, or
proof edge is missing, malformed, ambiguous, stale, or unproved.

TCTI implementation, ISA inventory, production diagnostics, and correctness
proof are kernel-owned and C-native. Production code and test-only observation
surfaces belong under `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix`.
KUnit is authoritative for decoder boundaries, lowering, exact register and
flag transitions, PC behavior, guest-memory effects, cache invariants, and
structured exits. Linux kselftest is authoritative for Linux-visible
integration reached through those exits. External-language scripts must not
implement or generate TCTI behavior, define the guest ISA inventory, model
architectural semantics, act as an instruction oracle, or replace KUnit or
kselftest evidence. Native debuggers and disassemblers may observe a failure,
but a durable regression and its proof must remain in the owning C test surface.

Guest ELF text pages remain host data mappings. Linux `VM_EXEC` remains meaningful, but TCTI enforces execute permission using Linux-owned VMA/PTE or Orlix arch/mm metadata. TCTI must not require JIT, MAP_JIT, RWX memory, generated executable memory, host executable page permissions for guest ELF text, Mach-O translated guest binaries, modified guest binaries, Wasm, or QEMU.

Executable-block construction and execution require the same Linux-owned mapping authorization. TCTI records the stable per-mm mapping generation while fetching and building a block, then revalidates and holds that generation while executing guest memory accesses. A PTE mutation invalidates the authorization; stale TLB or block-cache state must miss, retry, or fault rather than access a replacement mapping.

Guest writes commit to Linux-owned memory before the private host shadow is refreshed. If that postcommit refresh fails, OrlixKernel reports the failure and discards the affected host mapping. The discard path must not copy stale shadow data back into the authoritative Linux page.

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
- TCTI completeness requires full decode, lowering, and semantic coverage of the guest-exposed AArch64 EL0 ISA profile. Passing mlibc, Coreutils, or another workload does not establish ISA completeness.
- TCTI cache, translation, and execution diagnostics must expose structured
  kernel-owned state that focused KUnit can assert. Temporary host scripts are
  not an acceptable substitute for missing `arch/orlix` observability.
- Orlix-built packages remain Linux ELF binaries linked against OrlixMLibC.
- No public Orlix syscall facade is added.
- TCTI must call the existing `arch/orlix` Linux syscall dispatch path rather than adding a syscall emulator.
- Guest text may be read by TCTI as data, but never mapped executable by the host.
- An executable block is authorized only for the stable mapping generation observed during its construction and revalidated during execution.
- Postcommit host-refresh failure discards the stale host mapping without copying it back over committed Linux memory.
- Host-native Orlix components and Linux kernel semantics do not pass through TCTI.
- Direct native guest execution may exist only as a separate oracle or benchmark and cannot satisfy product-development, simulator-readiness, or release gates.
- TCTI performance claims require exact workload, device or simulator, build configuration, command, baseline, counters, and Markdown report.
- Runtime integration may use boot progress and source-preserving Linux
  console, PTY, HostAdapter, and host-VM diagnostics. Terminal text and
  human-readable logs do not prove TCTI instruction correctness when structured
  TCTI results, registers, guest memory, counters, or fault state are available.
  UIKit screen state is not a proof surface.

## Rejected Alternatives

- Compiling packages as iOS Mach-O app code for the primary runtime format.
- Routing Linux userspace syscalls to Darwin `svc #0`.
- Adding a custom Orlix Linux-like userspace ABI.
- Embedding libc, shell, VFS, fd, process, or signal behavior in TCTI.
- Moving Linux syscall/process/VFS policy into OrlixHostAdapter.
- External-language TCTI generators, architectural behavior models, instruction
  test oracles, and host-side substitutes for kernel KUnit or Linux kselftest.
- Treating QEMU, Wasm, JIT, MAP_JIT, RWX memory, generated executable memory, or host-executable guest text as the TestFlight/App Store path.
- Copying iSH, OpenMinis, or ios-linuxkit internals wholesale and renaming them Orlix.

`OrlixTCTI` is an Orlix-owned, arch/orlix, no-JIT, same-ISA, tail-call-threaded user-instruction backend for unmodified AArch64 Linux ELF binaries. It does not replace Linux; it lets OrlixKernel's existing Linux userspace surface run on iOS without host-executable guest text.
