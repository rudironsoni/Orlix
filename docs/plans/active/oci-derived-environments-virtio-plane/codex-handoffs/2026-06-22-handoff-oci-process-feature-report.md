# 2026-06-22 OCI Process Feature Report Handoff

Current checkpoint:

- `OrlixOCIRuntimeFeatureReport.current` now reports implemented process-default
  support for:
  - `process.user`
  - `process.noNewPrivileges`
  - `process.closeAdditionalFds`
  - `process.oomScoreAdj`
  - `process.scheduler`
  - `process.ioPriority`
  - `process.execCPUAffinity`
- Broad security/substrate surfaces remain rejected or absent rather than
  overclaimed.
- Focused feature-report proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_12-00-21-+0200.xcresult`
  with 3 passed tests.
- OCI descriptor/reporting regression proof passed in
  `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_12-01-21-+0200.xcresult`
  with 9 passed tests.

Non-claims:

- This does not prove full OCI Runtime compliance, real OCI process lifecycle,
  PID allocation, namespaces, cgroups, registry pull, `orlix run`, host-folder
  mounts, virtio-fs behavior, or runtime-observed process defaults inside a
  booted Linux process.
