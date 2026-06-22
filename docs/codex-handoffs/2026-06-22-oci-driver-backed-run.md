# OCI Driver-Backed Run Checkpoint

Date: 2026-06-22

Active plan:

`docs/plans/active/oci-derived-environments-virtio-plane/`

Current checkpoint:

- Added SPI `OrlixOCIRuntimeProcessSession.run(using:)`.
- The run helper composes driver-backed start and wait.
- Lifecycle state is advanced only from driver-returned observed process
  metadata.

Validation:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_16-29-51-+0200.xcresult
result=Passed
passedTests=2
failedTests=0
skippedTests=0
totalTestCount=2
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
