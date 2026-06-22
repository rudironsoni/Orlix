# 2026-06-22 OCI Supplementary Groups Handoff

Current checkpoint:

- `process.user.additionalGids` is now imported into
  `OrlixEnvironmentDescriptor.defaultSupplementaryGroups`.
- Root-image command lines emit indexed `orlix.suppgidN=<gid>` tokens.
- `OrlixOS/Sources/init/init.c` parses those tokens and calls `setgroups(2)`
  before `setgid(2)` and `setuid(2)`.
- Focused proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-06-10-+0200.xcresult`
  with 4 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-07-03-+0200.xcresult`
  with 8 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed supplementary groups inside a
  booted Linux process.
