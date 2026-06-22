# 2026-06-22 OCI No-New-Privileges Handoff

Current checkpoint:

- `process.noNewPrivileges` is now imported into
  `OrlixEnvironmentDescriptor.defaultNoNewPrivileges`.
- Root-image command lines emit `orlix.nonewprivs=1` when enabled.
- `OrlixOS/Sources/init/init.c` parses the key and calls
  `prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)` before credential drop and exec.
- Focused proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-25-41-+0200.xcresult`
  with 5 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_10-30-36-+0200.xcresult`
  with 8 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed `no_new_privs` inside a booted
  Linux process.
