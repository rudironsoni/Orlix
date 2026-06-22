# 2026-06-22 OCI OOM Score Adjustment Handoff

Current checkpoint:

- `process.oomScoreAdj` is now imported into
  `OrlixEnvironmentDescriptor.defaultOOMScoreAdjustment` when the value is in
  Linux's `-1000..1000` range.
- Root-image command lines emit signed `orlix.oomscoreadj=<value>` tokens when
  enabled.
- `OrlixOS/Sources/init/init.c` parses the signed key and writes it to
  `/proc/self/oom_score_adj` before credential drop and exec.
- Focused proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-54-25-+0200.xcresult`
  with 6 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-58-46-+0200.xcresult`
  with 8 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed `/proc/self/oom_score_adj`
  inside a booted Linux process.
