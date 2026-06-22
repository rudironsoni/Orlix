# Handoff - OCI Process Handle Boundary

Date: 2026-06-22

Current checkpoint:

- Added SPI `OrlixOCIRuntimeProcessHandle`.
- The handle binds an `OrlixOCIRuntimeLifecycleController` to the
  `OrlixOCIRuntimeSessionDescriptor` used for created/running OCI-derived
  Linux sessions.
- Added SPI `OrlixOCIRuntimeCompletedProcess` for stopped process results that
  expose state reporting without carrying a materializable session descriptor.
- Focused tests cover:
  - Created and running handles carrying matching session descriptor state.
  - `kill(signal:)` preserving a running session until observed completion.
  - Completed/stopped lifecycle records rejecting process-handle session
    materialization.
  - Configured lifecycle records rejecting process-handle materialization.

Validation:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_15-54-56-+0200.xcresult
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
