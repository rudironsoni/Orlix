---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-24
status: doing
summary: "Classify and implement the complete pinned AArch64 ISA target through Orlix TCTI."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
blocks:
  - "[Complete the pinned simulator TCTI ladder](complete-pinned-simulator-tcti-ladder.md)"
  - "[Promote TCTI as the product default](../todo/promote-tcti-as-product-default.md)"
targets:
  - "[TCTI](../../software-component/tcti.md)"
  - "[OrlixHostAdapter](../../software-component/orlixhostadapter.md)"
derived_from:
  - "[TCTI reference review](../../../sources/tcti/reference-review.md)"
  - "[ADR 0029](../../architecture-decision/0029-separate-complete-aarch64-target-from-runtime-profile.md)"
---

# Complete AArch64 ISA-on-ISA coverage

Implement 100% AArch64 ISA-on-ISA compatibility through Orlix-owned TCTI fetch, decode, lowering, data-only gadget execution, register and memory semantics, and structured exits under `arch/orlix`.

Completion requires:

- a C-native, build-time target inventory constructed from every one of the 4,350 leaves in the pinned Arm AARCHMRS 2026-06 source;
- an immutable 4,350-leaf direct instruction-encoding denominator from pinned `Instructions.json`, plus a supplemental reconciliation of pinned `Registers.json` system accessors to the generic `MRS`, `MSR`, and `SYS` leaves without inventing new instruction leaves; `RNDR` and `RNDRRS` remain access variants, not missing source leaves;
- SHA-256 and `_meta.version` validation for pinned `Instructions.json` (`a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe`), `Features.json` (`633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187`), and `Registers.json` (`5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874`), each with architecture `vFATAp1-A`, build `818`, reference `2026-06_rel`, schema `2.9.5`, and timestamp `2026-06-24 17:12:14`;
- a hard audit failure for every unmapped, directionally ambiguous, reserved, contradictory, or otherwise unrepresented `Accessors.SystemAccessor` selector space, with provenance from each accessor to its generic source leaf and no synthetic direct leaf or silent fallback;
- C-native production behavior, diagnostics, inventory, and correctness proof under `arch/orlix`, with no external-language generator, architectural behavior model, instruction oracle, or host-side substitute for owning KUnit and Linux kselftest;
- an explicit classification for every source leaf as a required EL0 instruction or semantic variant, privileged or non-EL0 behavior, architecturally undefined or unallocated, an alias or duplicate with a declared relationship, or a feature-conditioned alternative whose semantics remain in scope;
- the union of applicable EL0 feature configurations rather than one runtime feature evaluation, so optional-extension and feature-dependent semantic variants cannot disappear when their HWCAP is disabled;
- a separate runtime-profile projection that verifies Linux HWCAP and HWCAP2 advertise only capabilities already proved by the complete target inventory;
- a hard audit failure for every unclassified leaf, implicit exclusion, stale relationship, or source leaf missing from the target inventory;
- architectural decoders rather than workload-specific opcode lists;
- production gadget or lowering semantics for every required integer, control-flow, memory, atomic, SIMD, floating-point, crypto, optional-extension, and permitted system instruction;
- explicit LSE atomic coverage for CAS, CASP, and read-modify-write forms, including legal encoding boundaries, memory effects, atomicity, ordering, faults, and required Linux-visible behavior; `HWCAP_ATOMICS` may be advertised only after those complete target leaves pass their production-path KUnit and applicable kselftest proof;
- deterministic architectural handling of reserved, unallocated, privileged, unsupported-at-EL0, and not-yet-advertised optional-extension encodings;
- typed leaf-level KUnit evidence connecting each authoritative leaf to its expected decoder class, decoded operation, production lowering and execution path, legal encoding boundaries, register and flag transitions, memory effects, faults, PC changes, aliasing, atomicity, ordering, and exception results;
- executable-block construction, TLB lookup, and guest memory execution authorized by one stable per-mm mapping generation, with PTE mutations forcing stale cached authorization to miss, retry, or fault;
- postcommit host-refresh failure that reports the failed refresh and discards the stale host shadow without copying it back over the authoritative Linux page;
- Linux kselftest proof for representative live execution and Linux-visible integration without moving ISA assertions into XCTest or a host-side Swift gate;
- an independent coverage audit against the AArch64 architecture and the reference implementation inventory.

## Remaining sequence

1. Complete the exact feature-domain satisfiability audit. Evaluate `SAT(Features.json constraints && leaf condition)` for every pinned leaf across Boolean, integer, signed and unsigned width, enum and set membership, field and value, equality and inequality, ordering, implication, and equivalence semantics. Unsupported grammar, approximation, branch or resource exhaustion, and an unevaluated leaf are hard audit failures.
2. Preserve and classify every leaf conditioned by `FEAT_LOR`, `FEAT_LSUI`, `FEAT_LSE128`, `FEAT_THE`, and `FEAT_LSE2`. Each leaf requires exact applicable EL0 semantics or typed proof of its non-EL0, undefined or unallocated, alias or duplicate, or impossible-domain classification. Disabled HWCAP or HWCAP2 advertisement cannot remove these leaves from the target.
3. Reconcile every pinned `Accessors.SystemAccessor` to the generic `MRS`, `MSR`, or `SYS` leaf that carries its access direction and selector space. Reject any accessor that is unmapped, ambiguous, reserved, contradictory, or cannot retain source provenance. The reconciliation supplies supplemental system-access semantic variants and never changes the 4,350 direct instruction-encoding denominator. `RNDR` and `RNDRRS` are required variants of generic system access, not newly discovered instruction leaves.
4. Populate the C-native proof registry as classifications and evidence land. Every claimed proof must resolve to one registry entry with the correct proof identifier, instruction family, and allowed classification. Missing, unknown, duplicate, family-mismatched, or classification-mismatched references are hard audit failures, and the final registry cannot be empty.
5. Close the complete-target audit only after exact feature-domain evaluation covers all 4,350 leaves, all supplemental system-access variants reconcile to their generic leaves, the five named feature domains have complete classifications and required semantics or rejection proof, every proof reference resolves through the populated registry, and no leaf or accessor remains unclassified or unmapped.

The current runtime HWCAP profile may remain narrower while implementation is incomplete, but it cannot define the completion denominator or remove a leaf from blocking work. Until an extension is advertised, TCTI may reject its unadvertised encodings with the architecturally required EL0 behavior. That safety behavior does not complete the feature-conditioned target leaves. An extension capability may be advertised only after its complete target leaves and required Linux-visible behavior pass their owning proof.

Privileged and non-EL0 leaves remain visible in the inventory. Completion proves their architecturally correct EL0 rejection or exception behavior rather than silently discarding them.

Passing mlibc, Coreutils, or another package suite is downstream compatibility evidence. It does not close this task while any of the 4,350 source leaves is unclassified, any applicable EL0 leaf lacks exact production semantics and typed evidence, or any privileged leaf lacks its required EL0 behavior.
