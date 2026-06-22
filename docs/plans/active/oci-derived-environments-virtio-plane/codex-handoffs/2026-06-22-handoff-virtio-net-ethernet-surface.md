# 2026-06-22 Handoff - Virtio-Net Ethernet Surface

## Current Checkpoint

`virtio_net_device_probe` now proves that the upstream virtio-net device is
exposed as an Ethernet-shaped Linux netdev, without assuming the interface name
and without adding any custom ABI.

New Linux-surface assertions:

- `/sys/class/net/<ifname>/type` reports `ARPHRD_ETHER` (`1`).
- `/sys/class/net/<ifname>/addr_len` reports `6`.
- `RTM_GETLINK` reports the interface with a nonzero 6-byte hardware address.

Hosted XCTest assertions were updated to require the new TAP markers.

## Evidence

```sh
TMPDIR=/private/tmp rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
```

Packaging:

```text
Build/OrlixMLibC/kselftest/release/kselftest-list.txt:32:orlix:virtio_net_device_probe
Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list:39:file /orlix/virtio_net_device_probe ...
```

Focused hosted XCTest:

```sh
export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/opt/homebrew/bin"
rtk timeout 300 xcodebuild -quiet \
  -project OrlixSystem.xcodeproj \
  -scheme OrlixKernelUpstreamTests \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=E65F0D05-980C-4368-8CDC-2D2BF3E05757' \
  -only-testing:OrlixKernelUpstreamTests/OrlixKernelUpstreamTests/testVirtioNetDeviceProbeCompletesThroughOrlixOSTerminalSession \
  test
```

Result bundle:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-37-02-+0200.xcresult
```

Harness checks:

```text
rtk git diff --check
rtk python3 -m unittest discover .codex/hooks/tests  # 32 OK
rtk python3 -m unittest discover .codex/rules/tests  # 5 OK
rtk python3 .codex/hooks/compact_plan_check.py       # exit 0; stale-history warning only
```

## Non-Claims

This does not prove carrier/link-up behavior, packet I/O, DNS, NAT, registry
pull, OCI `netDevices`, or full OCI networking support.

## Next Work

Move the proof ladder from Ethernet-shaped netdev visibility to Linux network
behavior:

- Link/carrier/operstate semantics if the current backend can support a truthful
  claim.
- Packet-path proof only through standard Linux sockets/netdev/rtnetlink
  surfaces.
- Keep private Darwin/iOS mechanics inside HostAdapter and invisible to Linux
  userspace.
