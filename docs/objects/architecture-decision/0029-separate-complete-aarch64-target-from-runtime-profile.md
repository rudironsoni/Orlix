---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-24
status: accepted
external_id: "ADR-0029"
summary: "Separate the complete AArch64 ISA-on-ISA target inventory from the Linux runtime HWCAP projection."
part_of:
  - "[Orlix](../product/orlix.md)"
amends:
  - "[ADR 0022](0022-use-hosted-linux-elf-execution.md)"
targets:
  - "[TCTI](../software-component/tcti.md)"
---

# ADR 0029: Separate Complete AArch64 Target From Runtime Profile

## Status

Accepted.

## Context

ADR 0022 requires complete AArch64 EL0 ISA-on-ISA execution through Orlix
TCTI. A runtime profile derived from Linux `HWCAP` and `HWCAP2` remains
necessary to prevent Linux from advertising instructions that TCTI cannot yet
execute. That safety projection cannot define the completion target because
disabling a capability makes its instructions unreachable without implementing
or classifying them.

The pinned Arm source contains 4,350 direct instruction-encoding leaves in
`Instructions.json`. That immutable denominator counts instruction encodings,
not system-register names or access variants. Its semantics include
feature-conditioned alternatives, so evaluating the source once with absent
features set to false discards architectural variants that remain part of the
compatibility target.

## Decision

This decision changes only TCTI ISA completion accounting and capability
promotion. Every execution, ownership, and App Store boundary in ADR 0022
remains authoritative:

- ordinary unmodified AArch64 Linux ELF remains loaded by Linux `execve()` and
  `binfmt_elf`;
- TCTI remains limited to guest AArch64 EL0 instruction execution under
  `arch/orlix`;
- Linux retains syscall dispatch, VM, VFS, task, signal, wait, process, and
  exception-delivery semantics;
- guest text remains host data and must not require JIT, `MAP_JIT`, RWX,
  generated executable memory, or host-executable guest mappings;
- OrlixHostAdapter remains private Darwin mechanics and must not decode guest
  instructions, interpret Linux syscall policy, or own guest architectural
  semantics; and
- privileged or non-EL0 encodings receive their architecturally required EL0
  exception or structured TCTI exit and are never executed as privileged host
  operations.

TCTI maintains two separate ISA views:

- The **complete target inventory** is derived from all 4,350 leaves in the
  pinned Arm source and is authoritative for AArch64 ISA-on-ISA completion.
- The **runtime profile** is the current Linux `HWCAP` and `HWCAP2` projection.
  It is authoritative only for what Linux may advertise at that implementation
  checkpoint.

The complete target inventory models the union of applicable AArch64 EL0
feature configurations. Feature predicates and every alternative semantic
branch remain represented in the inventory. Inventory construction must not
select a single feature assignment and discard the branches that assignment
does not reach.

The audit reconciles a second, supplemental semantic source without changing
that 4,350-leaf denominator. Pinned `Registers.json` accessors derive the
system-register variants carried by the generic `MRS`, `MSR`, and `SYS` source
leaves. An accessor is not a new direct instruction leaf. For example,
`RNDR` and `RNDRRS` are system-register access variants reached through a
generic system-access encoding, not missing instruction leaves in
`Instructions.json`.

All three Arm source files are one pinned reconciliation contract. The audit
must validate their SHA-256 digests and `_meta.version` metadata before using
them:

- `Instructions.json`:
  `a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe`;
- `Features.json`:
  `633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187`; and
- `Registers.json`:
  `5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874`.

Each source must report architecture `vFATAp1-A`, build `818`, reference
`2026-06_rel`, schema `2.9.5`, and timestamp `2026-06-24 17:12:14`. A digest
or metadata mismatch is an audit failure, not a source fallback.

System-access reconciliation must fail loudly when an
`Accessors.SystemAccessor` cannot be mapped to its generic instruction leaf,
when its read or write direction cannot be represented, or when its encoded
selector space is missing, ambiguous, reserved, or contradicts the direct
instruction encoding. It must retain provenance from the accessor to the
generic leaf and preserve unsupported or privileged behavior as explicit
classification and rejection proof. It may not synthesize a direct leaf, drop
an accessor, or silently choose a selector mapping.

Every one of the 4,350 source leaves must have an explicit, auditable
classification:

- required EL0 instruction or semantic variant;
- privileged or otherwise non-EL0;
- architecturally undefined or unallocated;
- alias or duplicate representation, with an explicit relationship to its
  canonical leaf or representation;
- feature-conditioned alternative whose semantics remain required when its
  architectural feature conditions apply.

Classification records preserve feature predicates, alias or duplicate
relationships, and the required architectural behavior. A source leaf may not
disappear because a feature is absent from the current runtime profile. Any
unclassified leaf is a hard audit failure.

A blanket disabled-feature list is not a completion mechanism. An extension may
remain absent from the runtime `HWCAP` projection until it is implemented, but
its applicable EL0 leaves remain visible as blocking implementation gaps in the
complete target inventory. Runtime profile contraction can keep userspace safe;
it cannot reduce the target or convert missing semantics into completion.

Before an extension is advertised, the runtime profile may reject its
unadvertised instruction encodings with the architecturally required EL0
behavior. That rejection is a safety property of the narrower runtime profile.
It does not satisfy the target inventory: the feature-conditioned leaves remain
classified and blocking until their applicable semantics, legal encodings, and
required proofs are complete.

TCTI expands and proves coverage family by family. For each extension or
instruction family, the owning proof must:

1. decode every legal encoding and semantic variant;
2. reject reserved, undefined, and unallocated encodings with the required
   architectural behavior;
3. execute exact architectural semantics;
4. prove register state, memory effects, flags, PC transitions, faults, and
   ordering behavior through KUnit on the production TCTI path;
5. add Linux-visible kselftest when the extension changes behavior observable
   through Linux; and
6. enable the corresponding Linux `HWCAP` or `HWCAP2` capability only after
   those proofs pass.

Privileged and other non-EL0 leaves remain visible in the inventory and receive
direct rejection-behavior proof. Complete AArch64 compatibility does not permit
TCTI to execute privileged instructions at EL0. It requires TCTI to identify
their encodings and produce the architecturally required EL0 exception or
structured exit, allowing Linux to deliver the corresponding userspace
behavior. A generic silent drop or an unclassified unsupported path is not
acceptable.

## Consequences

- Completion means all 4,350 pinned direct instruction leaves are classified,
  every supplemental system-access variant is reconciled to a generic leaf,
  every applicable EL0 semantic variant is implemented and proved, and every
  non-executable classification has its required relationship or rejection
  proof.
- The runtime HWCAP projection may lag implementation safely, but it cannot
  make the completeness audit pass.
- Workload traces and the current runtime profile may prioritize families, but
  neither defines or narrows the target inventory.
- Feature-conditioned alternatives remain blocking until their semantics and
  legal encoding boundaries have owning proof.
- Capability advertisement is the final promotion step for a proved extension,
  not evidence that the extension is complete.

## Rejected Alternatives

- Defining ISA completion from the capabilities advertised by the current
  runtime profile.
- Treating disabled features as excluded from completeness accounting.
- Evaluating the Arm source under one feature assignment and discarding
  inactive branches.
- Allowing unclassified, privileged, reserved, undefined, unallocated, alias,
  or duplicate leaves to disappear from the audit.
- Treating a system-register accessor as a new direct instruction leaf, or
  silently dropping an unmapped accessor or selector space.
- Advertising a capability before production-path KUnit and applicable
  Linux-visible kselftest proof pass.
