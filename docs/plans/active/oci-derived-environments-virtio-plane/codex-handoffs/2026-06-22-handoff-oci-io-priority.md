# 2026-06-22 OCI I/O Priority Handoff

Current checkpoint:

- Supported OCI `process.ioPriority` class plus priority is now imported into
  `OrlixEnvironmentDescriptor.defaultIOPriority`.
- Root-image command lines emit `orlix.ioprio.class=<class>` and
  `orlix.ioprio.priority=<priority>` when enabled.
- `OrlixOS/Sources/init/init.c` parses the keys and calls Linux `ioprio_set(2)`
  through `SYS_ioprio_set` before credential drop and exec.
- Unknown classes and priorities outside `0...7` remain rejected.
- Focused proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_11-27-12-+0200.xcresult`
  with 6 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_11-31-50-+0200.xcresult`
  with 8 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed I/O priority inside a booted
  Linux process.
