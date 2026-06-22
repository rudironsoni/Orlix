# 2026-06-22 OCI CPU Affinity Handoff

Current checkpoint:

- Supported OCI `process.execCPUAffinity` masks are now imported into
  `OrlixEnvironmentDescriptor.defaultCPUAffinity`.
- Root-image command lines emit `orlix.cpuaffinity=<mask>` when enabled.
- `OrlixOS/Sources/init/init.c` parses comma-separated CPU indexes and ranges
  into `cpu_set_t` and calls Linux `sched_setaffinity(2)` before credential drop
  and exec.
- Malformed CPU lists and reversed ranges remain rejected.
- Focused proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_11-43-19-+0200.xcresult`
  with 6 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_11-48-05-+0200.xcresult`
  with 8 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed CPU affinity inside a booted
  Linux process.
