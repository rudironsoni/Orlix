# 2026-06-22 Handoff - Virtio-Net Rtnetlink Operstate

## Current Checkpoint

`virtio_net_device_probe` now proves that `RTM_GETLINK` reports a standard
Linux `IFLA_OPERSTATE` value for the virtio-net interface while preserving the
existing MTU consistency proof.

The first attempted assertion required `/sys/class/net/<ifname>/operstate`.
Hosted XCTest showed that is not currently exposed:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-54-26-+0200.xcresult
upstream failure marker found: not ok 7 - virtio-net netdev reports a standard operstate through sysfs
```

The green proof is deliberately narrower and truthful:

- `RTM_GETLINK` reports the virtio-net interface.
- `IFLA_MTU` matches `/sys/class/net/<ifname>/mtu`.
- `IFLA_OPERSTATE` is present and one of the standard Linux `IF_OPER_*` values.

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
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-59-02-+0200.xcresult
```

Harness checks:

```text
rtk git diff --check
rtk python3 -m unittest discover .codex/hooks/tests  # 32 OK
rtk python3 -m unittest discover .codex/rules/tests  # 5 OK
rtk python3 .codex/hooks/compact_plan_check.py       # exit 0; stale-history warning only
```

## Non-Claims

This does not prove sysfs operstate, carrier/link-up behavior, packet I/O, DNS,
NAT, registry pull, OCI `netDevices`, or full OCI networking support.

## Next Work

Continue through Linux networking semantics:

- Validate truthful carrier/link-up behavior only if the current backend
  exposes it through standard Linux surfaces.
- Add packet-path proof only through standard Linux sockets/netdev/rtnetlink
  surfaces.
- Keep private Darwin/iOS mechanics inside HostAdapter and invisible to Linux
  userspace.
