---
type: epic
tags: [epic, performance, orlix-tcti]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#241"
summary: "Maximize ARM64 execution performance without runtime code generation."
owned_by:
  - "[Orlix](../../product/orlix.md)"
has_story:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
  - "[Execute real Linux SMP](../../story/doing/execute-real-linux-smp.md)"
---

# Maximize ARM64 execution performance

Optimize data-only TCTI translation, memory access, Linux SMP, fixed signed helpers, virtio paths, supported Apple capacity signals, and measured code layout. Preserve exact Linux and AArch64 semantics. Never authorize JIT, `MAP_JIT`, RWX mappings, host-executable guest text, generated native code, Mach-O guest replacements, or a guest-visible Apple ABI. Every claim records workload, destination, profile, artifact, thermal state, and correctness boundary. Research issue `#259` is non-gating.
