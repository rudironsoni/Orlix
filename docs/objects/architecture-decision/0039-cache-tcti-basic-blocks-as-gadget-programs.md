---
type: architecture-decision
tags:
  - architecture
  - decision
  - orlix-tcti
updated: 2026-09-09
status: accepted
external_id: "ADR-0039"
summary: "Product TCTI caches straight-line A64 blocks as data-only C gadget programs and never emits host machine code."
part_of:
  - "[Orlix](../product/orlix.md)"
amends:
  - "[ADR 0022](0022-use-hosted-linux-elf-execution.md)"
targets:
  - "[OrlixTCTI](../software-component/orlixtcti.md)"
applies:
  - "[Hosted ELF and TCTI safety](../../concepts/hosted-elf-and-tcti-safety.md)"
---

# ADR 0039: Cache TCTI Basic Blocks As Gadget Programs

## Status

Accepted.

## Context

ADR 0022 forbids JIT, `MAP_JIT`, RWX, and generated executable pages for guest ELF text. Product TCTI must still run unmodified AArch64 Linux EL0 instructions. Fetching and decoding one instruction per dispatcher trip repeats decode work on hot loops. QEMU TCG and similar JITs emit host machine code and chain native blocks. That path needs executable generated memory and is not the App Store product contract.

The production engine already owns a data-only gadget program, a per-mm block cache, and a straight-line block builder under `arch/orlix/hosted_exec/orlix_tcti`.

## Decision

Product TCTI translates a straight-line guest block into a cached list of C gadget handlers and decoded payloads. It does not emit host machine code.

1. Decode from the current PC until a block-ending instruction, an unsupported or exception exit, a page boundary, or the block-size cap.
2. Lower each decoded instruction to a gadget function pointer plus payload words. The program is ordinary kernel data.
3. Insert the program in the block cache keyed by mm, guest start PC, and mapping generation.
4. On a later hit, execute the cached gadget list without fetching or decoding those instructions again.
5. Revalidate mapping generation while executing. A PTE mutation must miss, retry, or fault. Stale cached programs must not run.

Block-ending instructions remain control-flow, syscalls, breakpoints, undefined encodings, barriers, cache maintenance, and yield-class hints. Superinstruction fusion is optional later work. It must still produce data-only gadgets, not host text.

QEMU TCG-style native codegen, `MAP_JIT`, RWX, dual RW/RX aliases for generated code, and App Store JIT entitlements remain forbidden on the product path. Direct-native guest execution stays an oracle or benchmark only, as in ADR 0022.

This amends ADR 0022 only by naming the product translation unit: a cached gadget-program basic block. Linux ownership, OrlixTCTI ownership, ISA completeness, and executable-memory bans stay in force.

## Verification Gates

1. Production TCTI source under `arch/orlix/hosted_exec/orlix_tcti` builds and runs gadget programs from the block cache. It does not map guest text or generated buffers executable.
2. KUnit covers multi-instruction gadget programs, block-cache hit, generation bump, and mapping invalidation.
3. Product and simulator TCTI backends stay the same no-JIT path.

## Downstream Work

Hot-path speed work stays inside this contract: larger straight-line blocks, hotter local block slots, cheaper generation checks, or data-only superinstructions. A later ADR is required before any product JIT.
