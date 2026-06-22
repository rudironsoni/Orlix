# Handoff - Virtio-Net Device Probe

Date: 2026-06-22

## Scope

This checkpoint adds a dedicated Linux-owned `virtio_net_device_probe` kselftest
overlay program and packages it into the Orlix kselftest initramfs.

## Changed Files

- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/virtio_net_device_probe.c`
- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/Makefile`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`

## Behavior Covered By The Probe

- Finds a virtio-net device under `/sys/bus/virtio/devices` by standard device
  ID (`0x0001`, `0001`, or `1`).
- Finds the Linux netdev owned by that virtio device without assuming `eth0`.
- Requires `/sys/class/net/<ifname>` to exist.
- Requires `RTM_GETLINK` over `NETLINK_ROUTE` to enumerate that same non-loopback
  interface with a hardware address.
- Requires `/proc/net/dev` to report the interface.

## Validation

```text
TMPDIR=/private/tmp rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
result=passed

Build/OrlixMLibC/kselftest/release/kselftest-list.txt:32:orlix:virtio_net_device_probe
Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list:39:file /orlix/virtio_net_device_probe ...
```

## Runtime Status

App-hosted runtime proof is still pending. An attempted hosted XCTest replayed
the older `virtio_mmio_probe_contract` output instead of the new
`virtio_net_device_probe` output:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-00-02-+0200.xcresult
result=Failed
passedTests=0
failedTests=1
skippedTests=0
totalTestCount=1
expectedFailures=0
```

The unproven runner/XCTest wiring was removed from this checkpoint. Continue by
fixing or using a hosted path that runs
`orlix.kselftest=virtio_net_device_probe` in a fresh command-specific session.

## Non-Claims

This does not claim packet I/O, carrier state, DNS, NAT, registry pull, product
`orlix run`, OCI `netDevices`, or full OCI Runtime compliance.

