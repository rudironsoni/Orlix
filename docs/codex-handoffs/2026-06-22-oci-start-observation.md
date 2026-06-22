# OCI Start Observation Checkpoint

Date: 2026-06-22

Active plan:

`docs/plans/active/oci-derived-environments-virtio-plane/`

Current checkpoint:

- Added `OrlixOCIRuntimeProcessStartObservation`.
- Process handle/session start paths accept typed Linux-observed start metadata.
- Invalid observed PIDs fail before lifecycle/session state advances.
- `start(observedPID:)` remains as a compatibility wrapper over the typed
  observation.

Validation:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_16-19-22-+0200.xcresult
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
