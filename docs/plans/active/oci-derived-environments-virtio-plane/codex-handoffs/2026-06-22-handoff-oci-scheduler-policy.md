# 2026-06-22 OCI Scheduler Policy Handoff

Current checkpoint:

- Supported OCI `process.scheduler` policy plus priority is now imported into
  `OrlixEnvironmentDescriptor.defaultScheduler`.
- Root-image command lines emit `orlix.scheduler.policy=<policy>` and
  `orlix.scheduler.priority=<priority>` when enabled.
- `OrlixOS/Sources/init/init.c` parses the keys and calls
  `sched_setscheduler(2)` before credential drop and exec.
- Scheduler `nice`, `flags`, unknown policies, and invalid priorities remain
  rejected.
- Focused proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_11-08-49-+0200.xcresult`
  with 6 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_11-13-07-+0200.xcresult`
  with 8 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed scheduler state inside a
  booted Linux process.
