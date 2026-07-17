---
type: task
tags:
  - task
  - orlix-tcti
updated: 2026-07-17
status: todo
summary: "Complete architectural AArch64 EL0 ISA-on-ISA coverage in Orlix TCTI."
task_of:
  - "[Prove TCTI product execution](../../story/doing/prove-tcti-product-execution.md)"
blocks:
  - "[Promote TCTI as the product default](promote-tcti-as-product-default.md)"
targets:
  - "[TCTI](../../software-component/tcti.md)"
derived_from:
  - "[TCTI reference review](../../../sources/tcti/reference-review.md)"
---

# Complete AArch64 ISA-on-ISA coverage

Implement every architecturally valid AArch64 EL0 instruction in the guest-exposed ISA profile through Orlix-owned TCTI fetch, decode, lowering, data-only gadget execution, register and memory semantics, and structured exits under `arch/orlix`.

Completion requires:

- an explicit instruction-family and optional-extension inventory tied to the guest-visible ISA and HWCAP profile;
- architectural decoders rather than workload-specific opcode lists;
- production gadget or lowering semantics for integer, control-flow, memory, atomic, SIMD, floating-point, crypto, and permitted system instructions;
- deterministic architectural handling of reserved, unallocated, privileged, and unadvertised optional-extension encodings;
- KUnit proof for legal encoding boundaries, decode fields, lowering, register and flag transitions, memory effects, faults, PC changes, aliasing, and exception results;
- Linux kselftest proof for representative live execution and Linux-visible integration without moving ISA assertions into XCTest or a host-side Swift gate;
- an independent coverage audit against the AArch64 architecture and the reference implementation inventory.

Passing mlibc, Coreutils, or another package suite is downstream compatibility evidence. It does not close this task while any guest-exposed legal instruction remains unsupported or semantically reduced.
