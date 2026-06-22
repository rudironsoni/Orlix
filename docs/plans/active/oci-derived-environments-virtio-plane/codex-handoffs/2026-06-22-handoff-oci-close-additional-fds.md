# 2026-06-22 OCI Close Additional Fds Handoff

Current checkpoint:

- `process.closeAdditionalFds` is now imported into
  `OrlixEnvironmentDescriptor.defaultCloseAdditionalFds`.
- Root-image command lines emit `orlix.closefds=1` when enabled.
- `OrlixOS/Sources/init/init.c` parses the key and closes descriptors `>= 3`
  using an `RLIMIT_NOFILE`-bounded loop before credential drop and exec.
- Focused proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-39-53-+0200.xcresult`
  with 5 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-45-15-+0200.xcresult`
  with 8 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed fd closure inside a booted
  Linux process.
