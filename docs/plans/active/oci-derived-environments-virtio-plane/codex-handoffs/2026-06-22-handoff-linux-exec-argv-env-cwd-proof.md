# 2026-06-22 Handoff - Linux Exec Argv Env Cwd Proof

Current checkpoint:

- `process_lifecycle_probe` now proves an execed child observes Linux argv,
  environment, and cwd state.
- Parent forks, child changes cwd to `/tmp`, execs
  `/orlix/process_lifecycle_probe`, passes an `exec-defaults-argv0` argv vector
  and `ORLIX_EXEC_ENV=oci-defaults`, then parent verifies the exec image's
  sentinel exit status through `waitpid(2)`.
- Selected XCTest now requires `forked exec observes Linux argv env and cwd`.

Changed files:

- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/process_lifecycle_probe.c`
- `OrlixTestRunner/Tests/XCTest/OrlixKernelUpstreamTests/OrlixKernelUpstreamTests.swift`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`

Evidence:

```text
rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
result: passed
```

```text
Build/OrlixMLibC/kselftest/release/kselftest-list.txt:21:orlix:process_lifecycle_probe
Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list:28:file /orlix/process_lifecycle_probe ...
```

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_15-08-46-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

Non-claims:

- This proves Linux exec argv/env/cwd substrate only.
- It does not claim OCI Runtime `create/start/state/kill/delete` compliance.
- It does not claim product `orlix run`, registry pull, virtio-fs host-folder
  mounts, or full namespace/cgroup OCI integration.
