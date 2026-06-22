# Handoff - OCI Driver-Backed Run Composition

Date: 2026-06-22

Current checkpoint:

- Added SPI `OrlixOCIRuntimeProcessSession.run(using:)`.
- The run helper composes `start(using:)` and `wait(using:)` through the same
  observation driver.
- Focused tests prove the run path starts from a created lifecycle record, waits
  from a running record with the observed Linux PID, and stops from
  driver-returned observed completion metadata.

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
rtk python3 .codex/hooks/compact_plan_check.py
```

The compact-plan check exited `0` with the known stale-history/current-status
warnings.

Non-claims:

- No real OCI-created process launch, signal delivery, monitor, wait, or reap
  path is complete.
- No full OCI Runtime lifecycle compliance is claimed.
- No product `orlix run`, registry pull, virtio-fs host-folder mount behavior,
  networking, namespace, or cgroup integration is complete.
