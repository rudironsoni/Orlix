---
type: task
tags: [task, performance, measurement]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#242"
summary: "Establish reproducible Orlix runtime performance baselines."
task_of:
  - "[Optimize measured TCTI execution](../../story/doing/optimize-measured-tcti-execution.md)"
targets:
  - "[OrlixTCTI](../../software-component/orlixtcti.md)"
blocks:
  - "[Use the TCTI TLB hot path](use-tcti-tlb-hot-path.md)"
  - "[Eliminate cached decode work](eliminate-cached-decode-work.md)"
  - "[Keep guest state hot](keep-guest-state-hot.md)"
  - "[Execute real Linux SMP](../../story/doing/execute-real-linux-smp.md)"
  - "[Implement arch/orlix SMP foundations](implement-arch-orlix-smp-foundations.md)"
  - "[Tune Apple host CPU capacity](tune-apple-host-cpu-capacity.md)"
  - "[Scale virtio data paths](scale-virtio-data-paths.md)"
  - "[Research data-only Metal acceleration](research-data-only-metal-acceleration.md)"
---

# Establish runtime performance baselines

Measure decode, translations, guest memory, syscalls, host crossings, virtio, CPU scaling, and thermal state for loader startup, shell, packages, Git, Python, SQLite, OpenSSL, native builds, and multi-process work. Record destination, OS, profile, artifact, CPU count, thermal state, and exact workload. Keep simulator and device evidence separate. Measurements never replace correctness proof.
