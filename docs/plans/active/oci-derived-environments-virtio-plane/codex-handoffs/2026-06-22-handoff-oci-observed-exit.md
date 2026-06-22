# Handoff - OCI Observed Linux Process Exit

Date: 2026-06-22

Current checkpoint:

- Added `OrlixOCIRuntimeProcessExitObservation` in
  `OrlixOS/Sources/Session/OrlixOCIImageLayout.swift`.
- Added `OrlixOCIRuntimeLifecycleController.exit(observedProcess:)`.
- Validates observed PID and normal Linux exit status before lifecycle state is
  stopped from an observed process result.
- Rejects observed completion when the observed Linux PID does not match the
  running lifecycle record PID.
- Added focused OrlixOS tests in
  `OrlixOS/Tests/XCTest/OrlixOSTests/OrlixTerminalSessionTests.swift`.

Validation:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_15-41-08-+0200.xcresult
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

The compact-plan check exited `0` with existing stale-history/current-status
warnings before this handoff file was written.

Non-claims:

- No real OCI-created process launch, monitor, or reap path is complete.
- No full OCI Runtime lifecycle compliance is claimed.
- No product `orlix run`, registry pull, virtio-fs host-folder mount behavior,
  networking, namespace, or cgroup integration is complete.
