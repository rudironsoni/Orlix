# OCI Process Handle Checkpoint

Date: 2026-06-22

Active plan:

`docs/plans/active/oci-derived-environments-virtio-plane/`

Current checkpoint:

- Added SPI `OrlixOCIRuntimeProcessHandle` in OrlixOS.
- The handle binds `OrlixOCIRuntimeLifecycleController` to the
  `OrlixOCIRuntimeSessionDescriptor` used by created/running OCI-derived Linux
  sessions.
- Added SPI `OrlixOCIRuntimeCompletedProcess` for stopped process results that
  expose state reporting without carrying a materializable session descriptor.
- Focused tests cover created/running descriptor binding, kill preserving a
  running session until observed completion, and configured/stopped lifecycle
  rejection.

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
```

Non-claims:

- No real OCI-created process launch, signal delivery, monitor, wait, or reap
  path is complete.
- No product `orlix run`, registry pull, virtio-fs host-folder mount behavior,
  networking, namespace, or cgroup integration is complete.
