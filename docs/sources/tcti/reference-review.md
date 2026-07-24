---
type: source
tags:
  - provenance
updated: 2026-07-24
status: current
summary: "Canonical repository source for tcti reference review."
---

# TCTI reference review

This review records reference material used for the Orlix TCTI plan. The first implementation is clean-room/reference-only. No iSH, OpenMinis, or ios-linuxkit source is copied.

## Authority hierarchy and proof boundary

The pinned Arm AARCHMRS 2026-06 source is the sole authority for the 4,350-leaf TCTI completion target. Its instruction identifiers, encodings, feature predicates, register metadata, and allocation state define the target inventory. A public mirror of another release is useful for discovery, but it does not verify, replace, or establish provenance for the pinned 2026-06 source.

Pinned official Arm ASL is the semantic authority for each source leaf. Orlix records the applicable ASL semantic entry, including its decode predicates, side effects, exceptions, UNDEFINED behavior, and CONSTRAINED UNPREDICTABLE behavior. A leaf without an executable ASL entry requires an explicit source-backed disposition. Orlix does not translate ASL into TCTI or make ASL an execution-time dependency.

Linux arm64 is an integration reference only. It informs Linux-owned capability advertisement, exception delivery, ABI interaction, and normal Linux-visible behavior. It does not define TCTI instruction semantics or reduce the AARCHMRS target. Linux and KVM selftests are behavior references that help construct Linux-visible kselftests, but they do not replace production-path TCTI KUnit proof.

Sail, Isla, Islaris, and formal-validation papers are methodology references only. They motivate source-to-proof traceability, explicit state transitions, and allowed versus forbidden memory-ordering outcomes. They are not Orlix runtime dependencies, proof oracles, semantic generators, or completion authorities. OpenMinis, iSH, QEMU, Unicorn, LLVM, and binutils are non-authoritative comparison references. They can identify decoder families, aliases, edge cases, and test vectors, but cannot establish correctness or be copied into the Orlix execution model.

Every source leaf must have a machine-checkable provenance chain:

```text
pinned AARCHMRS release and source-leaf identifier
  -> retained EL0 feature predicate and ASL semantic entry
  -> classification and explicit alias relationship where applicable
  -> owning arch/orlix decoder and executor
  -> production-path KUnit state-transition evidence
  -> owning Linux kselftest where Linux-visible
  -> runtime HWCAP or HWCAP2 dependency when advertised
```

The target audit fails for a missing, unclassified, implicitly excluded, or unproved link in this chain. It also fails if a runtime capability reaches the Linux advertisement projection without complete owning evidence. Privileged and non-EL0 leaves remain visible, with the required EL0 rejection or exception behavior as their proof obligation.

External-language models, generators, host-side TCTI models, workload-defined subsets, exact-opcode shortcuts, log scraping, router logs, and terminal text are prohibited as ISA proof. TCTI implementation, inventory, diagnostics, and KUnit evidence remain C-native under `arch/orlix`; Linux-visible effects are proven through owning kselftests.

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
- Data-only gadget dispatch. The reviewed iSH Python generator remains provenance only and does not define, generate, or implement Orlix TCTI behavior.
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
- OpenMinis iSH ARM64: `https://github.com/OpenMinis/ish-arm64`, commits `a5e0a1e358e42a539ff916b4098628fd7f55e3fa` and `8932511fa0ab6abf77d5ead19503476d8b816f4f`.
- `https://github.com/OpenMinis/ish-arm64/blob/master/README_arm64.md`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/asbestos.c`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/asbestos.h`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gen.c`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/crypto_helpers.c`
- `https://github.com/OpenMinis/ish-arm64/tree/master/asbestos/guest-arm64/gadgets-aarch64`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gadgets-aarch64/bits.S`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gadgets-aarch64/control.S`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gadgets-aarch64/crypto.S`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gadgets-aarch64/entry.S`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gadgets-aarch64/gadgets.h`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gadgets-aarch64/math.S`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gadgets-aarch64/memory.S`
- `https://github.com/OpenMinis/ish-arm64/blob/master/emu/tlb.h`
- `https://github.com/OpenMinis/ish-arm64/blob/master/kernel/arch/arm64/calls.c`
- ios-linuxkit: `https://github.com/rcarmo/ios-linuxkit`, commit `312f1093bd008918036d845d0725a345f3bc342e`.
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/ARM64_BACKEND.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/RUNTIME_VALIDATION.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/ARM64_WORKLOAD_SMOKE_TESTS.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/LINUX_BUILD_AND_HOST_ABI.md`

## Secondary Comparison Notes

OpenMinis provides an independent inventory of decoder masks, modified-immediate expansion, crypto helpers, and gadget-family decomposition. Orlix uses that inventory to audit its own implementation. No OpenMinis production code or runtime ownership is adopted by this review.

- OpenMinis confirms the no-JIT model: basic blocks lower into arrays of gadget function pointers and operands. Execution tail-calls through precompiled gadget functions. No guest text becomes host-executable.
- OpenMinis is useful for performance targets: persistent TLB, block chaining, page-index invalidation, assembly hot paths, NEON and crypto expansion, and 48-bit guest VA pressure from Node, Go, Rust, and JVM workloads.
- OpenMinis also shows what Orlix must not copy as architecture: fakefs, syscall emulator, native offload, bind mounts as Linux semantics, DebugServer APIs, V8 binary patching, and app-level guest process ownership.
- ios-linuxkit is useful for validation discipline: bounded gates, Markdown reports, explicit pass/fail/unsupported rows, timeout as failure, and diagnostic lanes that are opt-in rather than mixed into clean runtime proof.
- Upstream iSH is useful only for the historical threaded-gadget concept and logging channels such as strace and verbose. Orlix must not inherit the x86 backend or upstream process/runtime model.

## Orlix Corrections From This Read

- The implementation should prioritize the TCTI pipeline and gadget lowering path over adding broad instruction semantics into the debug switch oracle.
- [CORRECTION] Workload traces may prioritize implementation and supply regression opcodes, but they do not define the supported ISA. TCTI completion requires an independent inventory of every legal instruction in the guest-exposed AArch64 EL0 profile, with production decode, lowering, gadget execution, architectural semantics, and deterministic exception behavior. Switch-debug coverage is allowed only as an oracle paired with the real TCTI path.
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

## Architecture encoding cross-checks

- Repository: `https://github.com/llvm/llvm-project`
- Commit reviewed: `f1073034a030a09bf0a29607aa0bee4430b31b97`
- Files reviewed:
  - `llvm/lib/Target/AArch64/AArch64InstrInfo.td`
  - `llvm/lib/Target/AArch64/AArch64InstrFormats.td`
- Use: confirm the A64 exception-generation field layout and the `op1` and `LL` assignments for `SVC`, `HVC`, `SMC`, `BRK`, `HLT`, and `DCPS1` through `DCPS3` before writing the independent Orlix KUnit boundary audit.
- No LLVM source was copied into Orlix.

### OpenMinis polynomial multiply cross-check

- Repository: `https://github.com/OpenMinis/ish-arm64`
- Commit reviewed: `89269e6fef7ab7aa61b133deae90d78e34a09ed1`
- Files reviewed:
  - `asbestos/guest-arm64/gen.c`
  - `asbestos/guest-arm64/gadgets-aarch64/crypto.S`
- Use: independently confirm the `PMUL`, `PMULL`, and `PMULL2` encoding families, legal element sizes, `Q` source-half selection, and carryless-multiply decomposition before completing the Orlix-owned KUnit proof.
- No OpenMinis source was copied into Orlix.

### OpenMinis AES cross-check

- Repository: `https://github.com/OpenMinis/ish-arm64`
- Commit reviewed: `89269e6fef7ab7aa61b133deae90d78e34a09ed1`
- Files reviewed:
  - `asbestos/guest-arm64/crypto_helpers.c`
  - `asbestos/guest-arm64/gadgets-aarch64/crypto.S`
- Use: independently cross-check the `AESE`, `AESD`, `AESMC`, and `AESIMC` instruction decomposition, standard S-box values, ShiftRows direction, and MixColumns coefficients before completing the Orlix-owned KUnit proof.
- The Orlix test uses standard AES lookup constants as an oracle independent of the production algebra. No OpenMinis production code, control flow, or runtime ownership was copied into Orlix.
