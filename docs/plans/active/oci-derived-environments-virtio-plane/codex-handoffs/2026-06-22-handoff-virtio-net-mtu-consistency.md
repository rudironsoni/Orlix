# 2026-06-22 Handoff - Virtio-Net MTU Consistency

## Current Checkpoint

`virtio_net_device_probe` now proves MTU consistency through standard Linux
surfaces:

- `/sys/class/net/<ifname>/mtu` reports a positive MTU.
- `RTM_GETLINK` reports `IFLA_MTU`.
- The rtnetlink MTU matches the sysfs MTU for the same virtio-net interface.

The probe still avoids interface-name assumptions and custom ABI. It remains a
Linux-owned kselftest under the Orlix kernel overlay.

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
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-46-35-+0200.xcresult
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

Continue from Linux network behavior, not host plumbing:

- Validate truthful carrier/link/operstate semantics if the current virtio-net
  backend supports them.
- Add packet-path proof only through standard Linux sockets/netdev/rtnetlink
  surfaces.
- Keep private Darwin/iOS mechanics inside HostAdapter and invisible to Linux
  userspace.
