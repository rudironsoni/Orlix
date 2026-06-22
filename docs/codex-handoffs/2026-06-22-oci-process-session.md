# OCI Process Session Checkpoint

Date: 2026-06-22

Active plan:

`docs/plans/active/oci-derived-environments-virtio-plane/`

Current checkpoint:

- Added SPI `OrlixOCIRuntimeProcessSession`.
- The process session binds an OCI process handle to an existing
  `OrlixLinuxSession`.
- The process session does not create or materialize a root image implicitly.
- Lifecycle updates preserve the same Linux session until observed process
  completion returns `OrlixOCIRuntimeCompletedProcess`.

Validation:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_16-13-01-+0200.xcresult
result=Passed
passedTests=2
failedTests=0
skippedTests=0
totalTestCount=2
expectedFailures=0

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_16-13-40-+0200.xcresult
result=Passed
passedTests=3
failedTests=0
skippedTests=0
totalTestCount=3
expectedFailures=0
```

Harness:

```text
rtk git diff --check
rtk python3 -m unittest discover .codex/hooks/tests
rtk python3 -m unittest discover .codex/rules/tests
```

Non-claims:

- No real OCI-created process launch, signal delivery, monitor, wait, or reap
  path is complete.
- No product `orlix run`, registry pull, virtio-fs host-folder mount behavior,
  networking, namespace, or cgroup integration is complete.
