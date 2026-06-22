# OCI Process Capabilities Handoff - 2026-06-22

Checkpoint commit should include bounded OCI `process.capabilities` support.

Implemented:

- `OrlixEnvironmentCapabilities` in `OrlixOS/Sources/Session/OrlixEnvironment.swift`.
- OCI parser support for canonical Linux `CAP_*` names in:
  - `bounding`
  - `permitted`
  - `inheritable`
  - `effective`
  - `ambient`
- Invalid capability names still fail closed with field-specific
  `unsupportedLinuxFeature("process.capabilities.<set>")`.
- Init command-line keys:
  - `orlix.cap.bounding`
  - `orlix.cap.permitted`
  - `orlix.cap.inheritable`
  - `orlix.cap.effective`
  - `orlix.cap.ambient`
- Linux init application in `OrlixOS/Sources/init/init.c`:
  - bounding drops with `prctl(PR_CAPBSET_DROP, ...)`
  - permitted/effective/inheritable with `capget(2)` and `capset(2)`
  - ambient with `prctl(PR_CAP_AMBIENT, ...)`
  - `PR_SET_KEEPCAPS` around UID/GID changes when final capability sets are
    requested
- `OrlixOCIRuntimeFeatureReport.current` now reports bounded
  `process.capabilities` support.

Evidence:

- `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_13-02-23-+0200.xcresult`
  - focused OCI capabilities/defaults group
  - `result=Passed`, `passedTests=6`, `failedTests=0`, `skippedTests=0`,
    `totalTestCount=6`, `expectedFailures=0`
- `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_13-03-50-+0200.xcresult`
  - broader OCI descriptor/reporting group
  - `result=Passed`, `passedTests=9`, `failedTests=0`, `skippedTests=0`,
    `totalTestCount=9`, `expectedFailures=0`
- Harness:
  - `rtk git diff --check`
  - `rtk python3 -m unittest discover .codex/hooks/tests` (`32` tests)
  - `rtk python3 -m unittest discover .codex/rules/tests` (`5` tests)
  - `rtk python3 .codex/hooks/compact_plan_check.py` exited `0` with the known
    stale-history warning.

Non-claims:

- No runtime-observed capability proof inside a booted Linux process yet.
- No OCI process lifecycle/PID/reaping proof.
- No namespace, cgroup v2, registry pull, product `orlix run`, virtio-fs
  host-folder mount, or full OCI Runtime compliance claim.
