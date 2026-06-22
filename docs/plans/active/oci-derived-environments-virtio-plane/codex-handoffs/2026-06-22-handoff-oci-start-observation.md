# Handoff - OCI Start Observation

Date: 2026-06-22

Current checkpoint:

- Added `OrlixOCIRuntimeProcessStartObservation`.
- Process handle/session start paths now accept typed Linux-observed start
  metadata through `start(observedProcess:)`.
- `start(observedPID:)` remains as a compatibility convenience and constructs
  the typed observation before lifecycle mutation.
- Focused tests cover valid typed start observations through process handle and
  process session paths, plus invalid observed PID rejection.

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
