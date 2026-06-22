# 2026-06-22 Handoff - Virtio-Net Ioctl Flags

## Current Checkpoint

`virtio_net_device_probe` now proves consistency between `RTM_GETLINK` and
legacy `SIOCGIFFLAGS` for the virtio-net interface.

New assertion:

- Capture `ifi_flags` from `RTM_GETLINK`.
- Call `SIOCGIFFLAGS` for the same interface.
- Compare `IFF_UP`, `IFF_RUNNING`, and `IFF_LOOPBACK` bits.

This keeps the proof inside standard Linux networking surfaces and does not add
any custom ABI or host-visible escape hatch.

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
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_19-08-26-+0200.xcresult
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

The next meaningful jump is carrier/link-up or packet-path behavior. Do that
only through standard Linux netdev/socket/rtnetlink surfaces. If the current
virtio-net backend cannot truthfully support it yet, fix the owning Linux-side
virtio/MMIO path instead of adding HostAdapter-visible ABI.
