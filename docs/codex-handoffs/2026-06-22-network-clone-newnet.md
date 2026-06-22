# Handoff - Network CLONE_NEWNET Isolation Proof

Date: 2026-06-22

## Summary

The Orlix Linux networking selftest now proves that a child process can enter an
isolated network namespace through `SYS_unshare(CLONE_NEWNET)` and still observe
Linux-owned `/proc/net` and rtnetlink surfaces.

## Files

- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/network_namespace_probe.c`
- `OrlixTestRunner/Tests/XCTest/OrlixKernelUpstreamTests/OrlixKernelUpstreamTests.swift`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`
- `docs/plans/active/oci-derived-environments-virtio-plane/codex-handoffs/2026-06-22-handoff-network-clone-newnet.md`

## Evidence

```text
TMPDIR=/private/tmp rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
result=passed

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_17-21-32-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

## Next Direction

Continue networking work through Linux-owned interfaces: namespace-local route
mutation/error semantics or virtio-net device/packet proof. Do not expose Darwin
or simulator details to the Linux surface.

