# Handoff - OCI Process Session Binding

Date: 2026-06-22

Current checkpoint:

- Added SPI `OrlixOCIRuntimeProcessSession`.
- The process session binds `OrlixOCIRuntimeProcessHandle` to an existing
  `OrlixLinuxSession`.
- The binding is explicit and does not silently materialize a missing root
  image from lifecycle state.
- `start(observedPID:)` and `kill(signal:)` preserve the same Linux session
  while updating the OCI process handle.
- Observed normal/signal completion returns `OrlixOCIRuntimeCompletedProcess`.

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
