# 2026-06-22 Handoff - Linux Fork/Exec/Wait Proof

Current checkpoint:

- Commit target: Linux fork/exec/wait lifecycle substrate.
- `process_lifecycle_probe` now forks, execs `/orlix/process_lifecycle_probe`
  with `exec-child`, reads `/proc/self/status` in the exec image, exits with a
  sentinel status, and verifies that the parent observes that status through
  `waitpid(2)`.
- Selected XCTest now requires the `forked child exec status is reported by
  waitpid` marker.

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
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_15-00-36-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

Harness:

```text
rtk git diff --check
rtk python3 -m unittest discover .codex/hooks/tests
rtk python3 -m unittest discover .codex/rules/tests
```

Known warning:

```text
ORLIX-HARNESS-WARN: docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md has stale pending/blocked status that appears contradicted by later green status
```

Non-claims:

- This proves Linux fork/exec/wait substrate only.
- It does not claim OCI Runtime `create/start/state/kill/delete` compliance.
- It does not claim product `orlix run`, registry pull, virtio-fs host-folder
  mounts, or full namespace/cgroup OCI integration.
