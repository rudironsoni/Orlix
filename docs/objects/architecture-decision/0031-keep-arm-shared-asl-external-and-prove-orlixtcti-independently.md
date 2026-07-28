---
type: architecture-decision
tags:
  - architecture
  - decision
  - orlix-tcti
updated: 2026-07-28
status: accepted
external_id: "ADR-0031"
summary: "Keep official Arm shared ASL as external hash-pinned provenance and prove independently authored OrlixTCTI behavior."
part_of:
  - "[Orlix](../product/orlix.md)"
derived_from:
  - "[Official Arm shared-ASL source research](../../sources/tcti/shared-asl-source-research.md)"
amends:
  - "[ADR 0022](0022-use-hosted-linux-elf-execution.md)"
  - "[ADR 0029](0029-separate-complete-aarch64-target-from-runtime-profile.md)"
targets:
  - "[OrlixTCTI](../software-component/orlixtcti.md)"
---

# ADR 0031: Keep Arm Shared ASL External And Prove OrlixTCTI Independently

## Status

Accepted.

## Context

ADR 0022 and ADR 0029 require the pinned Arm AARCHMRS 2026-06 source and a separately pinned official Arm shared-ASL corpus to define the complete OrlixTCTI source-to-proof graph. They also require the shared-ASL body, decode, and transitive helper provenance for every applicable target leaf.

Arm publishes the official `ISA_A64_xml_A_profile-2026-06.tar.gz` archive. It contains `shared_pseudocode.xml`, `sharedps.dtd`, instruction XML, and `notice.xml`. The archive and relevant members are technically pin-able and compatible with the `2026-06` AARCHMRS release as recorded in the [source research](../../sources/tcti/shared-asl-source-research.md).

The archive notice does not itself verify an implementation or redistribution license. [UNVERIFIED] No traceable authorization is recorded in this repository that permits retaining or redistributing the XML or shared-ASL bodies, translating them into Orlix implementation, or generating implementation from them. This decision is an engineering and repository-distribution boundary, not legal advice. A later applicable agreement or express permission may justify a new decision.

The absence of verified authorization must not reduce the complete AArch64 target, turn unsupported semantics into success, or make third-party implementations authoritative.

## Decision

The immutable completion denominator remains all 4,350 direct instruction leaves in the pinned Arm AARCHMRS 2026-06 `Instructions.json`, reconciled with the pinned `Features.json` and `Registers.json` contract established by ADR 0029. Every applicable AArch64 EL0 semantic variant remains an implementation and proof obligation. Every non-executable, privileged, reserved, undefined, unallocated, alias, or duplicate leaf remains an explicit classification and behavior obligation.

The official versioned DDI0602 `2026-06` A64 ISA documentation and its `ISA_A64_xml_A_profile-2026-06.tar.gz` archive remain the external, non-redistributed semantic specification. AARCHMRS owns inventory and applicability; DDI0602 owns the expected architectural behavior. The archive identity is:

- archive URL and release `2026-06`;
- archive SHA-256 `63a01a1696483bbe2edfef9e0f0cd053d6c1c619ec0587876cb7a60bb344f354`;
- `shared_pseudocode.xml` SHA-256 `21edeadbc26408a35bcf7315bfbcbb8a69e9e6d86d952c80b153d273f0f2349f`;
- `sharedps.dtd` SHA-256 `84fd11a935d6e51a0b866f4982332d34f3b3fe9a0c03b1b3e9cac0a56d472395`; and
- `notice.xml` SHA-256 `9c2cc480e9706819e0d295329cf7f94f9256c610dfa9bc8f055a05389f74704a`.

Canonical C artifacts may retain the archive identity, DDI0602 document or corpus-relative locator identifiers, and cryptographic digests needed to detect missing, stale, malformed, ambiguous, or mismatched semantic provenance. They must not contain XML or ASL bodies, extracted pseudocode, translated pseudocode, generated semantic implementation, or copied helper implementation unless traceable authorization is recorded and a later accepted decision changes this boundary. Normal kernel, KUnit, kselftest, product, and audit builds remain independent of the external archive.

OrlixTCTI semantics are independently authored in the owning C-native production implementation under `arch/orlix`. The checkable semantic contract is the observable architectural behavior of each leaf and variant, including:

- legal and reserved encoding boundaries;
- register, flag, PC, and bounded memory state transitions;
- loads, stores, atomicity, ordering, and fault behavior where applicable;
- structured syscall, breakpoint, undefined-instruction, privilege, and exception exits; and
- the Linux-visible result delivered through the existing OrlixKernel path.

Each obligation must bind its pinned AARCHMRS identity and applicability condition, its exact external DDI0602 document or ASL locator and digest, its independently authored production owner, explicit expected observations derived from that official specification, production-path KUnit owner and executed result, applicable Linux kselftest owner and executed result or typed `not_applicable` reason, and runtime `HWCAP` or `HWCAP2` dependency. Orlix-authored implementation and tests cannot be their own semantic authority. A source locator or digest proves provenance only; the cited official specification defines the expected behavior, while execution evidence proves the Orlix implementation produced it.

OpenMinis, Linux arm64, KVM selftests, QEMU, LLVM, binutils, Sail, Isla, Islaris, Unicorn, hardware observations, and other external implementations may guide independent implementation, differential diagnosis, and test selection. They are non-authoritative implementation references. Their code, generated output, model output, or logs must not replace the pinned AARCHMRS inventory, satisfy a leaf's implementation or proof obligation, or decide runtime capability advertisement. Orlix must not copy or mechanically translate OpenMinis or another implementation into OrlixTCTI as a substitute for independently owned code.

This amends ADR 0022 and ADR 0029 only where they require redistributable shared-ASL bodies, exact ASL body publication, or shared-ASL helper closure as the semantic implementation authority. Their Linux ownership, OrlixTCTI ownership, executable-memory safety, fixed 4,350-leaf denominator, complete feature-domain, system-accessor reconciliation, runtime-profile separation, and proof boundaries remain in force.

## Verification Gates

The contract fails closed unless all of these conditions hold:

1. The external Arm archive identity and every retained locator or digest match the pinned `2026-06` provenance contract.
2. Repository and generated release artifacts contain no Arm XML or ASL body and no implementation mechanically generated or translated from that body without recorded authorization.
3. Every one of the 4,350 direct leaves and every reconciled system-access semantic variant has exactly one complete classification and source-bound applicability result.
4. Every applicable EL0 variant has an independently authored production implementation and exact observable-behavior obligations executed on the production path through owning KUnit.
5. Every Linux-visible obligation has executed owning kselftest evidence or a source-bound typed `not_applicable` reason.
6. Every non-executable leaf has direct proof of its required EL0 rejection, exception, alias, or duplicate behavior.
7. The completion audit reports zero missing, stale, malformed, ambiguous, unsupported, or unproved obligations without treating absent authorization, absent ASL bodies, disabled runtime features, third-party results, or generic unsupported handling as success.
8. Linux advertises an `HWCAP` or `HWCAP2` capability only after every dependency required by ADR 0029 passes its owning production-path and Linux-visible proof.

## Downstream Work

GitHub issue #117 must publish the external source identity and DDI0602 or corpus-relative locator identifiers and digests permitted by this repository contract, then bind every leaf to independently authored observable-behavior obligations and production owners. It must remove exact ASL body, extracted decode body, and shared-helper body publication from its acceptance contract. It also owns a complete migration inventory for every source and test sentinel that currently treats absent ASL bodies as the blocker, including crypto, MOPS, SME, pointer-authentication, and the completion audit. Each sentinel must become either external-specification provenance or a real missing implementation or proof obligation. Missing, stale, malformed, incompatible, dangling, or ambiguous external provenance must still fail closed, while absence of redistributed ASL text is required rather than an error.

GitHub issue #156 must make the authoritative C-native audit enforce this decision. The audit must retain the exact 4,350-leaf denominator, reject any missing classification, applicability, production owner, observable-behavior obligation, executed KUnit result, applicable kselftest result, or advertisement edge, and verify that external ASL provenance receives no implementation or proof credit. A zero-error audit remains metadata and evidence validation; it does not replace execution of KUnit or Linux kselftest.

## Consequences

- Issue #112's revised architecture contract is satisfied without claiming an unverified license or redistributing Arm source bodies.
- Complete AArch64 compatibility remains the target. This decision changes the semantic-authority and repository-distribution contract, not the denominator or required behavior.
- The official Arm XML and shared-ASL package remains traceable external provenance, but its presence, locator coverage, or digest match cannot close an implementation or proof obligation.
- Orlix owns the production semantics and the executable proof for every target leaf. Missing semantics remain visible blockers.
- A later verified authorization may permit stronger source retention or generated provenance only through a new accepted decision and corresponding repository-license review.

## Rejected Alternatives

- Redistributing the Arm XML or shared-ASL bodies without traceable authorization.
- Generating OrlixTCTI implementation from the external ASL corpus under the current repository contract.
- Shrinking the 4,350-leaf target to the current runtime profile or to implemented families.
- Treating disabled `HWCAP` or `HWCAP2` features, a generic unsupported exit, or missing ASL text as completed semantics.
- Making OpenMinis, QEMU, LLVM, Sail, another implementation, generated model output, hardware logs, or inferred behavior the authoritative completion source.
