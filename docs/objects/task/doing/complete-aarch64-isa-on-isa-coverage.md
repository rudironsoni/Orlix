---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-08-28
status: doing
summary: "Classify, implement, and prove all 4,350 pinned AArch64 ISA leaves through OrlixTCTI."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
blocks:
  - "[Complete the pinned simulator TCTI ladder](complete-pinned-simulator-tcti-ladder.md)"
  - "[Promote TCTI as the product default](../todo/promote-tcti-as-product-default.md)"
targets:
  - "[OrlixTCTI](../../software-component/orlixtcti.md)"
  - "[OrlixHostAdapter](../../software-component/orlixhostadapter.md)"
derived_from:
  - "[TCTI reference review](../../../sources/tcti/reference-review.md)"
  - "[Official Arm shared-ASL source research](../../../sources/tcti/shared-asl-source-research.md)"
  - "[ADR 0029](../../architecture-decision/0029-separate-complete-aarch64-target-from-runtime-profile.md)"
  - "[ADR 0031](../../architecture-decision/0031-keep-arm-shared-asl-external-and-prove-orlixtcti-independently.md)"
---

# Complete AArch64 ISA-on-ISA coverage

Implement 100% AArch64 ISA-on-ISA compatibility through Orlix-owned TCTI fetch, decode, lowering, data-only gadget execution, register and memory semantics, and structured exits under `arch/orlix`.

Completion requires:

- a C-native, build-time target inventory constructed from every one of the 4,350 leaves in the pinned Arm AARCHMRS 2026-06 source;
- canonical fixed-width C artifacts under `arch/orlix` as the only ISA
  inventory inputs to normal kernel builds, KUnit, kselftest, product builds,
  and `tcti-isa-audit`, with no JSON parser, JSON file, or external Arm-source
  path in those dependency graphs;
- an explicit maintainer-refresh path that alone reads the pinned Arm JSON,
  validates the complete three-source contract, and atomically publishes a
  coherent C artifact set without replacing the previous authoritative set
  after failure;
- Kconfig used only for capability and build selection, never as the inventory
  store, generator, filter, or completion denominator;
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
- a machine-checkable source-to-proof contract for every direct leaf. Each
  record binds the pinned bundle, ordinal, AARCHMRS identifier, family, alias
  path, source spans, condition AST and digest, `operation_id`, external ASL
  locator and digest, classifications, independent implementation owner, KUnit
  and kselftest owners and results, and runtime capability dependency. External
  ASL grants no implementation or proof credit, and no Arm XML or ASL body may
  enter repository artifacts under ADR 0031. Inapplicable edges require typed
  `not_applicable`; each `operational_note` retains its digest and explicit
  behavior and proof obligation. A matching-release DDI0602 omission is valid
  source accounting only when AARCHMRS records `// Not specified` and the row
  retains that exact locator and digest; it remains a semantic blocker;
- explicit applicability and ownership statuses for every leaf and every
  feature-conditioned semantic variant. Applicability is one of
  `applicable_el0`, `non_el0`, `undefined_or_unallocated`,
  `alias_or_duplicate`, or `impossible_domain`, and an
  `impossible_domain` result requires a checked unsatisfiability witness from
  the complete feature domain. Required EL0 variants must name their owning
  production decoder and executor symbols, KUnit owner, and applicable
  kselftest owner. Aliases and duplicates must name a canonical source leaf
  and state whether the relationship is an encoding, decode, semantic, or
  assembler-only alias;
- a hard audit failure for a missing, stale, malformed, ambiguous, or
  non-canonical source, semantic, applicability, implementation, proof, alias,
  operational-note, or advertisement edge. The audit must also fail when an
  external ASL locator or digest, source span, condition branch, required result
  field, or Linux-visible proof obligation required by ADR 0031 is absent;
- Arm AARCHMRS as the authority for the complete ISA inventory and
  applicability contract, and external versioned DDI0602 documentation as the
  official semantic specification. Independent OrlixTCTI behavior and
  production-path proof must bind exact external locators and digests; Linux
  owns Linux-visible interfaces and exception delivery. External
  implementations, including OpenMinis, may inform independent implementation,
  tests, and diagnosis, but cannot replace Arm authority, receive implementation
  or proof credit, become a runtime dependency, or decide capability
  advertisement;
- executable-block construction, TLB lookup, and guest memory execution authorized by one stable per-mm mapping generation, with PTE mutations forcing stale cached authorization to miss, retry, or fault;
- postcommit host-refresh failure that reports the failed refresh and discards the stale host shadow without copying it back over the authoritative Linux page;
- Linux kselftest proof for representative live execution and Linux-visible integration without moving ISA assertions into XCTest or a host-side Swift gate;
- an independent coverage audit against the AArch64 architecture and the reference implementation inventory.

## Remaining sequence

1. Complete the maintainer-refresh pipeline and the canonical fixed-width C
   artifact contract. The refresh path alone reads and validates the three
   pinned Arm JSON sources, then atomically publishes the complete artifact set
   under `arch/orlix`. Prove that normal kernel, KUnit, kselftest, product, and
   `tcti-isa-audit` dependency graphs contain no JSON parser, JSON file, or
   external source path.
2. Complete the exact feature-domain satisfiability audit from the canonical C
   artifacts. Evaluate the retained equivalent of
   `SAT(Features.json constraints && leaf condition)` for every pinned leaf
   across Boolean, integer, signed and unsigned width, enum and set membership,
   field and value, equality and inequality, ordering, implication, and
   equivalence semantics. Unsupported grammar, approximation, branch or
   resource exhaustion, and an unevaluated leaf are hard audit failures.
3. Preserve and classify every leaf conditioned by `FEAT_LOR`, `FEAT_LSUI`, `FEAT_LSE128`, `FEAT_THE`, and `FEAT_LSE2`. Each leaf requires exact applicable EL0 semantics or typed proof of its non-EL0, undefined or unallocated, alias or duplicate, or impossible-domain classification. Disabled HWCAP or HWCAP2 advertisement cannot remove these leaves from the target.
4. Reconcile every pinned `Accessors.SystemAccessor` to the generic `MRS`, `MSR`, or `SYS` leaf that carries its access direction and selector space. Reject any accessor that is unmapped, ambiguous, reserved, contradictory, or cannot retain source provenance. The reconciliation supplies supplemental system-access semantic variants and never changes the 4,350 direct instruction-encoding denominator. `RNDR` and `RNDRRS` are required variants of generic system access, not newly discovered instruction leaves.
5. Populate the C-native semantic and proof registry as classifications and
   evidence land. Every direct leaf and semantic variant must resolve through
   its complete source-to-proof contract, including source spans, effective
   condition, ASL and operational-note provenance where present, explicit
   applicability, canonical alias relation where applicable, production owner,
   KUnit owner, Linux-visible owner where applicable, and runtime-advertisement
   dependency. Missing, unknown, duplicate, family-mismatched,
   classification-mismatched, condition-mismatched, or stale references are
   hard audit failures, and the final registry cannot be empty.
6. Close the complete-target audit only after the canonical C artifact pipeline is closed, exact feature-domain evaluation covers all 4,350 leaves, all supplemental system-access variants reconcile to their generic leaves, the five named feature domains have complete classifications and required semantics or rejection proof, each ASL and operational-note edge is source-bound, every proof reference resolves through the populated registry, and no leaf, semantic variant, accessor, owner, or required proof obligation remains unclassified, unmapped, or unproved.

The current runtime HWCAP profile may remain narrower while implementation is incomplete, but it cannot define the completion denominator or remove a leaf from blocking work. Until an extension is advertised, TCTI may reject its unadvertised encodings with the architecturally required EL0 behavior. That safety behavior does not complete the feature-conditioned target leaves. An extension capability may be advertised only after its complete target leaves and required Linux-visible behavior pass their owning proof.

Privileged and non-EL0 leaves remain visible in the inventory. Completion proves their architecturally correct EL0 rejection or exception behavior rather than silently discarding them.

Passing mlibc, Coreutils, or another package suite is downstream compatibility evidence. It does not close this task while any of the 4,350 source leaves is unclassified, any applicable EL0 leaf lacks exact production semantics and typed evidence, or any privileged leaf lacks its required EL0 behavior.

## Landed cohort state

| Family | Leaves | Current proof and blockers |
| --- | ---: | --- |
| `BASE_LOAD_STORE` | 209 | Typed production observations cover registers, memory, PC, and faults. |
| `ADVSIMD_LOAD_STORE` | 152 | The same memory proof covers all leaves. STL1 and LDAP1 prove structure transfer only. |
| `BASE_ATOMICS` | 498 | Success observations cover registers, memory, and PC, plus atomicity and ordering where required. |
| `BASE_ADD_SUBTRACT` | 34 | Registers, PC, and flags are proved. ADDPT and SUBPT remain here. `FEAT_CPA` is not advertised. |
| `BASE_CONTROL_FLOW` | 10 | Five EL0 leaves are proved. Five privileged leaves prove rejection. `FEAT_TEV` is not advertised. |
| `BASE_EXCEPTIONS` | 10 | Four leaves prove structured exits. Six privileged leaves prove rejection. Linux owns syscall dispatch. |
| `BASE_CONDITIONAL` | 60 | Twenty-three always-on leaves are proved. The other 37 prove rejection. `FEAT_HBC` and `FEAT_CMPBR` are not advertised. |
| `BASE_BITFIELD_UNARY` | 25 | Nineteen always-on leaves are proved. Six `FEAT_CSSC` leaves prove rejection. `FEAT_CSSC` is not advertised. |
| `BASE_MULTIPLY_DIVIDE` | 16 | All leaves and edge cases are proved. `FEAT_CPA` is not advertised. |
| `ADVSIMD_CRYPTO` | 32 | AES, SHA, SM3, SM4, and PMULL are proved. Crypto HWCAP bits are not advertised. |
| `ADVSIMD_PERMUTE_MOVE` | 37 | The 33 always-on leaves and edge cases are proved. Four `FEAT_LUT` leaves prove EL0 rejection. `FEAT_LUT` is not advertised. |
| `ADVSIMD_INTEGER` | 240 | The 222 always-on leaves and edge cases are proved. Eighteen RDM, DotProd, and I8MM leaves prove EL0 rejection. Their HWCAP bits are not advertised. |
| `ADVSIMD_FP` | 268 | The 113 always-on leaves are proved, including NaN, infinity, zero, subnormal, rounding, IOC, Q=0, and alias cases. The 155 FP16, FHM, BF16, FCMA, FRINTTS, FP8, FAMINMAX, and FSCALE leaves remain complete-target blockers. Their capability bits, including `HWCAP_FPHP` and `HWCAP_ASIMDHP`, are not advertised. |
| `SCALAR_FP` | 266 | All 140 FEAT_FP S/D leaves are proved. Before ingest, host mnemonics check results, FPCR, NZCV, and saved registers. Coverage includes sticky FPSR, quiet-NaN compare IOC, FABS qNaN/sNaN payload and IOC, FCVT qNaN/sNaN IOC/DN, FRINTI/FRINTX IXC, FDIV DZC, FZ subnormal IDC, signed-zero min/max, conditions, aliases, poison, and FMOV lanes. The 126 FP16, FPRCVT, FRINTTS, JSCVT, and BF16 leaves remain unadvertised target blockers. |

Runtime rejection does not complete optional leaves. Pointer-auth branches, BTI,
SVE, and SME remain open. These cohorts do not close the 4,350-leaf audit.

## Audit foundation

- All 4,350 direct leaves retain source spans, ASL absence witnesses,
  operational-note provenance, and reconciled system accessors. Registration is
  ownership only. Proof requires one executed production result per ordinal and
  obligation.
- The V3 field domain resolves 605 occurrences in 362 groups to 606 unambiguous
  alternatives. Typed values keep widths from 1 through 128 bits. The alias
  graph has 292 instruction and 171 reachable operation aliases. Aliases do not
  replace leaves or proof.
- The runtime cohort has 5,592 unresolved memberships over 409 feature
  parameters. Linux proof has 6,364 rows: 3,314 kselftest-owned, 3,050 typed
  N/A, and zero executed.
- SAT classifies all 4,350 leaves as applicable, with zero impossible or
  unresolved. Its certificate has 377 common and 364 leaf-scoped values.
- LSE128 fault coverage proves no atomicity or ordering. HWCAP and HWCAP2
  promotion remains zero. Independently authored semantics and execution proof
  remain open.
