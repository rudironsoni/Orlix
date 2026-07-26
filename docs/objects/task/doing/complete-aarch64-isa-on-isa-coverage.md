---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-26
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
  - "[ADR 0029](../../architecture-decision/0029-separate-complete-aarch64-target-from-runtime-profile.md)"
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
  record must retain the pinned three-source bundle identity, source ordinal,
  AARCHMRS identifier, instruction family, complete inheritance and alias path,
  encoding and operand source span, effective feature-condition AST and
  canonical digest, effective `operation_id`, ASL semantic locator, shared
  decode and operation digests, EL classification, semantic classification,
  canonical alias or duplicate relationship, implementation owner,
  implementation status, KUnit owner, applicable Linux-visible kselftest owner,
  proof status, and runtime `HWCAP` or `HWCAP2` dependency. Inapplicable
  kselftest or runtime-advertisement edges require explicit typed
  `not_applicable` dispositions rather than missing values;
- a separately pinned Arm shared-ASL corpus identity for every semantic record,
  including its release, digest, corpus-relative entry locator, normalized
  source digest, and referenced shared-helper identifiers. `operation_id`
  alone is not ASL provenance. Any non-empty AARCHMRS `operational_note` must
  have a separate authoritative-note digest and an explicit behavior and proof
  obligation, because the source defines it as behavior that ASL cannot
  express;
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
  ASL reference, source span, helper dependency, condition branch, required
  result field, or Linux-visible proof obligation is absent;
- Arm AARCHMRS and the pinned Arm shared-ASL corpus as the sole authority for
  ISA inventory and semantic expectations. Linux remains authoritative for
  Linux-visible interfaces and exception delivery. Sail, Isla, Islaris, QEMU,
  Unicorn, LLVM, binutils, KVM selftests, OpenMinis, hardware experiments, and
  terminal or log output may inform test selection or investigation only. They
  must never supply completion semantics, replace an Arm source edge, become a
  runtime dependency, or decide an HWCAP or HWCAP2 advertisement;
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

The current audit foundation retains raw source spans for all 4,350 direct
leaves, binds all pinned inline operation objects as explicit shared-ASL
absence witnesses with explicit operational-note provenance, authenticates
the selector and condition identity of all supplemental system accessors
without changing the direct-leaf denominator, and treats KUnit and kselftest
source registration as ownership only. Native execution remains unproved
until a typed result binds each source ordinal and individual obligation to
an executed production-path case. Runtime HWCAP and HWCAP2 projection
requires the complete 4,350-leaf provider and remains disabled for every
unproved extension.

The current checkpoint preserves all 605 feature-field qualifier occurrences
in the checked feature artifact. The maintainer binding core derives 362
identity groups, maps 604 occurrences to an explicit field-domain relation,
and retains one ambiguous occurrence as a blocking audit failure. The runtime
projection rejects a stale zero-profile mapping,
and archive-freshness regression coverage rejects a kernel archive that is
older than durable `arch/orlix` inputs. A narrow typed native-observation
comparator now records result, register, and bounded-memory observations, but
it discharges no proof obligation without owning production-path KUnit run
evidence. LSE128 reserved-selector and near-miss coverage remains pending
owning run evidence and its remaining architectural obligations stay blocking.

The current typed feature evaluation retains fixed-width values from 1 through
128 bits. The checked field-domain artifact keeps all 605 occurrences, its 604
structural mappings remain semantically unresolved, and the `MPAM` ambiguity
remains a hard blocker. The direct denominator remains 4,350 leaves while the
supplemental alias graph records 292 instruction aliases and 171 reachable
operation aliases without using aliases as proof or denominator substitutions.
The pinned 2026-06 package has no separately pin-able official shared-ASL
corpus, so all 4,350 shared-ASL provenance edges remain blocking. The LSE128
production resume fault matrix has expanded its fault coverage, but it makes no
atomicity or ordering claim. Runtime HWCAP and HWCAP2 promotion remains zero.

The checked runtime-capability cohort artifact now covers all 4,350 direct
instruction leaves and retains 5,592 source-derived candidate memberships over
the 409 typed feature parameters, including each candidate's exact source
condition span. Every membership remains explicitly unresolved. The completion
audit counts those 5,592 unresolved memberships as blocking obligations, and
the runtime projection cannot use them to authorize HWCAP or HWCAP2 promotion.
This structural binding does not establish feature applicability,
satisfiability, implementation, or proof. Those obligations, the 605
feature-field qualifier semantics, the missing shared-ASL provenance edges,
and the per-family KUnit and applicable kselftest evidence remain open.
