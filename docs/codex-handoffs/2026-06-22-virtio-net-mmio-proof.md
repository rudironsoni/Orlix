# Handoff - Virtio-Net MMIO Visibility Proof

Date: 2026-06-22

## Summary

The app-hosted virtio MMIO contract XCTest now requires the existing Linux
selftest markers for upstream virtio bus visibility and virtio-net device
visibility.

## Files

- `OrlixTestRunner/Tests/XCTest/OrlixKernelUpstreamTests/OrlixKernelUpstreamTests.swift`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`
- `docs/plans/active/oci-derived-environments-virtio-plane/codex-handoffs/2026-06-22-handoff-virtio-net-mmio-proof.md`

## Evidence

```text
Build/OrlixMLibC/kselftest/release/kselftest-list.txt:31:orlix:virtio_mmio_probe_contract
Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list:38:file /orlix/virtio_mmio_probe_contract ...

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_17-35-27-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

## Next Direction

Continue toward a dedicated Linux-owned virtio-net interface or packet proof.
Do not expose Darwin or simulator networking details to the Linux surface.

