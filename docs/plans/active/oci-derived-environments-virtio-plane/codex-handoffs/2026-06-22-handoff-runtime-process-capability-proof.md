# Runtime Process Capability Proof Handoff - 2026-06-22

Checkpoint commit should include app-hosted Linux capability substrate proof for
OCI `process.capabilities`.

Implemented:

- Added `process_capability_probe` to the durable Orlix kselftest overlay:
  `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/process_capability_probe.c`.
- Registered the probe in:
  `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/Makefile`.
- Added selected runtime test spec:
  `OrlixUpstreamTestRunSpec.kernelProcessCapability`.
- Added selected XCTest:
  `OrlixKernelUpstreamTests/testProcessCapabilityProbeCompletesThroughOrlixOSTerminalSession`.
- Updated `OrlixOCIRuntimeFeatureReport.current` so `process.capabilities`
  cites `proof: "orlix:process_capability_probe"`.

Probe coverage:

- `capget(2)` reads current capability sets.
- `/proc/self/status` exposes `CapInh`, `CapPrm`, `CapEff`, `CapBnd`, and
  `CapAmb`.
- `capset(2)` accepts current capability sets.
- Clearing effective capabilities through `capset(2)` is observed in
  `/proc/self/status`.
- `prctl(PR_CAPBSET_READ, ...)` reads a standard Linux capability from the
  bounding set.
- `prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, ...)` is observed in
  `/proc/self/status`.

Evidence:

- `rtk make -f OrlixKernel/Makefile kselftest PROFILE=release`
- `Build/OrlixMLibC/kselftest/release/kselftest-list.txt` contains
  `orlix:process_capability_probe`.
- `Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list`
  contains `/orlix/process_capability_probe`.
- `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_13-25-04-+0200.xcresult`
  - app-hosted selected kselftest
  - `result=Passed`, `passedTests=1`, `failedTests=0`, `skippedTests=0`,
    `totalTestCount=1`, `expectedFailures=0`
- `/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixOSTests-2026.06.22_13-30-22-+0200.xcresult`
  - focused feature-report tests
  - `result=Passed`, `passedTests=2`, `failedTests=0`, `skippedTests=0`,
    `totalTestCount=2`, `expectedFailures=0`
- Harness:
  - `rtk git diff --check`
  - `rtk python3 -m unittest discover .codex/hooks/tests` (`32` tests)
  - `rtk python3 -m unittest discover .codex/rules/tests` (`5` tests)
  - `rtk python3 .codex/hooks/compact_plan_check.py` exited `0` with the known
    stale-history warning.

Non-claims:

- This is Linux capability substrate proof inside a booted Orlix process, not
  full runtime-observed proof that imported OCI `config.json` process defaults
  have been applied to an OCI process.
- OCI lifecycle, PID allocation/reaping, namespaces, cgroup resource behavior,
  registry pull, product `orlix run`, virtio-fs host-folder mounts, and full OCI
  Runtime compliance remain open.
