# Handoff - Network CLONE_NEWNET Isolation Proof

Date: 2026-06-22

## Scope

This checkpoint extends the Linux-owned `network_namespace_probe` coverage from
rtnetlink state enumeration to network namespace isolation.

## Changed Files

- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/network_namespace_probe.c`
- `OrlixTestRunner/Tests/XCTest/OrlixKernelUpstreamTests/OrlixKernelUpstreamTests.swift`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`

## Behavior Proved

- A forked child calls `SYS_unshare(CLONE_NEWNET)`.
- The child verifies `/proc/self/ns/net` changes after unshare.
- The child verifies `/proc/net/dev`, `/proc/net/tcp`, and `/proc/net/udp`
  remain readable inside the new network namespace.
- The child verifies a local `NETLINK_ROUTE` rtnetlink socket still opens inside
  the new network namespace.
- The app-hosted XCTest requires these markers:
  - `network namespace child enters isolated net namespace`
  - `new network namespace keeps procfs network state readable`
  - `new network namespace keeps rtnetlink socket local`

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

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_17-21-32-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

## Non-Claims

This does not claim full OCI networking, virtio-net packet I/O, registry pull,
product `orlix run`, complete cgroup integration, or full OCI Runtime
compliance.

