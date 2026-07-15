---
type: source
tags:
  - provenance
updated: 2026-07-15
status: current
summary: "Canonical repository source for tcti reference review."
---

# TCTI reference review

This review records reference material used for the Orlix TCTI plan. The first implementation is clean-room/reference-only. No iSH, OpenMinis, or ios-linuxkit source is copied.

## Primary Reference

- Repository: `https://github.com/rudironsoni/ish`
- Branch: `feat/aarch64-migration`
- Commit reviewed: `55d14a9fefe47a7ed9b3bb44e4ccca7429bd9363`
- Files reviewed:
  - `README.md`
  - `project.yml`
  - `docs/plans/a64-tcti-proof-program.md`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/cpu.c`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/cpu.h`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/block-cache.h`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/block-cache.c`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/fetch.h`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/fetch.c`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/memory.c`
  - `Sources/IXLandLinuxRuntime/emu/aarch64/sysreg.c`
  - `Sources/IXLandLinuxRuntime/emu/mmu.h`
  - `Sources/IXLandLinuxRuntime/emu/tlb.h`
  - `Sources/IXLandLinuxRuntime/emu/tlb.c`
  - `Sources/IXLandLinuxRuntime/tcti/frame.h`
  - `Sources/IXLandLinuxRuntime/tcti/aarch64/gen.h`
  - `Sources/IXLandLinuxRuntime/tcti/aarch64/gen.c`
  - `Sources/IXLandLinuxRuntime/tcti/aarch64/tcti-gadget-gen.py`
  - `Sources/IXLandLinuxRuntime/kernel/memory.c`
  - `Sources/IXLandLinuxRuntime/kernel/page_map.c`

## Concepts Carried Into Orlix

- AArch64 guest-only TCTI path.
- No product fallback interpreter for guest execution. A debug switch path may exist only as a correctness oracle.
- Generated gadget table from a source-of-truth generator, with deterministic generated output.
- Data-only gadget stream.
- `tcti_entry_block(gadgets, cpu_state)`-style entry shape.
- Generation-stamped TLB and block cache entries.
- Separate translation generation and code generation.
- Block cache keyed by guest PC plus executable-code generation.
- Persistent per-execution context caches and counters for performance measurement.
- Host `PROT_READ` for guest executable pages, never host `PROT_EXEC`.
- Fetch/decode/lowering/dispatch/exit instrumentation vocabulary.
- Contract/system/perf/UI test separation.
- Instruction family coverage matrix and proof ladder: decode, lowering, semantic execution, live guest evidence, perf evidence.

## Concepts Not Copied Blindly

- IXLand runtime ownership model.
- iSH process model, fakefs, syscall emulator, native offload, bind-mount shortcuts, DebugServer/app agent API.
- x0-x12-only hot register mapping as a final design.
- Hardcoded ldso PC-range diagnostics.
- O(n) whole-cache range invalidation.
- No-lock block cache.
- Fixed 1024-entry TLB.
- READ/FETCH permission conflation.
- x0-x12 hot register mapping as the final performance design.
- Memory-backed x13-x30 as an unmeasured final performance design.
- SIGSEGV-based recovery as the milestone-1 memory model.
- Host runtime syscall ownership. In Orlix, `svc #0` exits to OrlixKernel syscall entry, not to a HostAdapter or app runtime syscall emulator.

## Secondary References

- Upstream iSH: `https://github.com/ish-app/ish`, commit `997642f3787cc63e65f7134b7bb0362c74bff8e0`.
- OpenMinis iSH ARM64: `https://github.com/OpenMinis/ish-arm64`, commit `a5e0a1e358e42a539ff916b4098628fd7f55e3fa`.
- `https://github.com/OpenMinis/ish-arm64/blob/master/README_arm64.md`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/asbestos.c`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/asbestos.h`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gen.c`
- `https://github.com/OpenMinis/ish-arm64/tree/master/asbestos/guest-arm64/gadgets-aarch64`
- `https://github.com/OpenMinis/ish-arm64/blob/master/emu/tlb.h`
- `https://github.com/OpenMinis/ish-arm64/blob/master/kernel/arch/arm64/calls.c`
- ios-linuxkit: `https://github.com/rcarmo/ios-linuxkit`, commit `312f1093bd008918036d845d0725a345f3bc342e`.
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/ARM64_BACKEND.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/RUNTIME_VALIDATION.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/ARM64_WORKLOAD_SMOKE_TESTS.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/LINUX_BUILD_AND_HOST_ABI.md`

## Secondary Comparison Notes

- OpenMinis confirms the no-JIT model: basic blocks lower into arrays of gadget function pointers and operands. Execution tail-calls through precompiled gadget functions. No guest text becomes host-executable.
- OpenMinis is useful for performance targets: persistent TLB, block chaining, page-index invalidation, assembly hot paths, NEON and crypto expansion, and 48-bit guest VA pressure from Node, Go, Rust, and JVM workloads.
- OpenMinis also shows what Orlix must not copy as architecture: fakefs, syscall emulator, native offload, bind mounts as Linux semantics, DebugServer APIs, V8 binary patching, and app-level guest process ownership.
- ios-linuxkit is useful for validation discipline: bounded gates, Markdown reports, explicit pass/fail/unsupported rows, timeout as failure, and diagnostic lanes that are opt-in rather than mixed into clean runtime proof.
- Upstream iSH is useful only for the historical threaded-gadget concept and logging channels such as strace and verbose. Orlix must not inherit the x86 backend or upstream process/runtime model.

## Orlix Corrections From This Read

- The implementation should prioritize the TCTI pipeline and gadget lowering path over adding broad instruction semantics into the debug switch oracle.
- Opcode expansion remains trace-led, but every new opcode should land in decoder plus lowering/gadget contracts. Switch-debug coverage is allowed only as an oracle paired with the real TCTI path.
- Orlix must keep the stronger plan requirement that `FETCH`, `READ`, and `WRITE` are separate access classes. The primary branch's fetch-through-read behavior is a known weakness to avoid.
- Orlix block invalidation should move beyond whole-cache scans before serious chaining. Page-index reverse lookup remains a required design point.
- Orlix performance claims require counters and workload reports. No speed claim follows from the reference design or from a compile-only check.

## Future Source-Copy Rule

Any future copied source requires recording:

- source repository
- source file
- source commit
- license
- attribution requirement
- GPL compatibility with the OrlixKernel/Linux distribution path
- App Store distribution implications
- reason clean-room implementation is insufficient
