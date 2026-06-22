# Handoff - OCI Kill Uses Observed Signal Termination

Date: 2026-06-22

Current checkpoint:

- `OrlixOCIRuntimeLifecycleController.kill(signal:)` no longer fabricates a
  stopped lifecycle record with `128 + signal`.
- `kill(signal:)` now validates signal delivery against a running record and
  leaves lifecycle state `running`.
- Added `OrlixOCIRuntimeProcessSignalObservation`.
- Added `exit(observedSignal:)` so stopped signal state comes from an observed
  Linux process termination result with matching PID.
- Signal values are bounded to `1...127` to keep signal-derived exit status
  values inside the Linux process exit-status range.
- Focused tests cover kill remaining running, observed signal termination,
  mismatched observed PID rejection, and invalid signal rejection.

Validation:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_15-47-37-+0200.xcresult
result=Passed
passedTests=4
failedTests=0
skippedTests=0
totalTestCount=4
expectedFailures=0

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_15-48-20-+0200.xcresult
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

The compact-plan check exited `0` with the known stale-history warning.

Non-claims:

- No real OCI-created process launch, signal delivery, monitor, wait, or reap
  path is complete.
- No full OCI Runtime lifecycle compliance is claimed.
- No product `orlix run`, registry pull, virtio-fs host-folder mount behavior,
  networking, namespace, or cgroup integration is complete.
