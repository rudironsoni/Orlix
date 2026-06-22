# 2026-06-22 Handoff - OCI Stopped Session Boundary

Current checkpoint:

- `OrlixOCIRuntimeLifecycleController.sessionDescriptor(rootMount:)` now rejects
  stopped lifecycle records with `.stateUnavailable(.stopped)`.
- Created/running records can still produce session descriptors.
- Configured/stopped/deleted records now fail closed for session materialization.
- Added `testOCIRuntimeLifecycleControllerRejectsStoppedSessionDescriptor`.

Changed files:

- `OrlixOS/Sources/Session/OrlixOCIImageLayout.swift`
- `OrlixOS/Tests/XCTest/OrlixOSTests/OrlixTerminalSessionTests.swift`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`

Evidence:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_15-26-44-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

Non-claims:

- This guards lifecycle/session boundaries only.
- It does not launch, monitor, or reap a real OCI-created process object.
- It does not claim full OCI Runtime `create/start/state/kill/delete`
  compliance, product `orlix run`, registry pull, virtio-fs host-folder mounts,
  or full namespace/cgroup OCI integration.
