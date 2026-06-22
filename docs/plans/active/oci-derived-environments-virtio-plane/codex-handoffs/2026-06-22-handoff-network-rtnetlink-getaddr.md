# Handoff - Network RTM_GETADDR Loopback Proof

Date: 2026-06-22

## Scope

This checkpoint extends the Linux-owned `network_namespace_probe` coverage from
link enumeration to address enumeration.

## Changed Files

- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/network_namespace_probe.c`
- `OrlixTestRunner/Tests/XCTest/OrlixKernelUpstreamTests/OrlixKernelUpstreamTests.swift`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`

## Behavior Proved

- `network_namespace_probe` now sends a standard `RTM_GETADDR` dump request over
  `NETLINK_ROUTE`.
- The probe requires an `RTM_NEWADDR` entry for `127.0.0.1/8` after loopback is
  configured through Linux socket ioctls.
- The app-hosted XCTest requires the marker:
  `RTM_GETADDR reports loopback IPv4 address`.

## Validation

```text
TMPDIR=/private/tmp rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
result=passed

Build/OrlixMLibC/kselftest/release/kselftest-list.txt:15:orlix:network_namespace_probe
Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list:22:file /orlix/network_namespace_probe ...

export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/opt/homebrew/bin"
rtk xcode-storage-doctor
rtk timeout 300 xcodebuild -quiet \
  -project OrlixSystem.xcodeproj \
  -scheme OrlixKernelUpstreamTests \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=E65F0D05-980C-4368-8CDC-2D2BF3E05757' \
  -only-testing:OrlixKernelUpstreamTests/OrlixKernelUpstreamTests/testNetworkNamespaceProbeCompletesThroughOrlixOSTerminalSession \
  test

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_17-04-54-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

## Non-Claims

This does not claim full OCI networking, virtio-net packet I/O, registry pull,
product `orlix run`, complete namespace/cgroup integration, or full OCI Runtime
compliance.

