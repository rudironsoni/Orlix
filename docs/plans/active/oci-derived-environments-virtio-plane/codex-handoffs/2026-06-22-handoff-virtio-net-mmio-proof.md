# Handoff - Virtio-Net MMIO Visibility Proof

Date: 2026-06-22

## Scope

This checkpoint hardens the app-hosted `virtio_mmio_probe_contract` XCTest so it
requires the existing Linux selftest markers for upstream virtio bus visibility
and the virtio-net device.

## Changed Files

- `OrlixTestRunner/Tests/XCTest/OrlixKernelUpstreamTests/OrlixKernelUpstreamTests.swift`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`

## Behavior Proved

- The focused hosted XCTest now requires:
  - `upstream virtio bus exposes devices`
  - `upstream virtio bus exposes the virtio-net device`
- Existing packaged kselftest evidence still includes:
  - `orlix:virtio_mmio_probe_contract`
  - `/orlix/virtio_mmio_probe_contract`

## Validation

```text
Build/OrlixMLibC/kselftest/release/kselftest-list.txt:31:orlix:virtio_mmio_probe_contract
Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list:38:file /orlix/virtio_mmio_probe_contract ...

export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/opt/homebrew/bin"
rtk xcode-storage-doctor
rtk timeout 300 xcodebuild -quiet \
  -project OrlixSystem.xcodeproj \
  -scheme OrlixKernelUpstreamTests \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=E65F0D05-980C-4368-8CDC-2D2BF3E05757' \
  -only-testing:OrlixKernelUpstreamTests/OrlixKernelUpstreamTests/testVirtioMMIOContractProbeCompletesThroughOrlixOSTerminalSession \
  test

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_17-35-27-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

## Non-Claims

This does not claim virtio-net packet I/O, external networking, DNS, NAT,
registry pull, product `orlix run`, or full OCI Runtime compliance.

