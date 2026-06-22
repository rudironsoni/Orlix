# 2026-06-22 Handoff - Hosted Virtio-Net Device Proof

## Current Checkpoint

- Added `OrlixUpstreamTestRunSpec.kernelVirtioNetDevice`.
- Added `testVirtioNetDeviceProbeCompletesThroughOrlixOSTerminalSession`.
- Changed hosted upstream test terminal identifiers to include the sanitized
  kernel command-line suffix, preventing `.kernel` kselftests from sharing the
  same persisted `orlix.test.kernel.terminal` transcript.
- Confirmed the dedicated hosted run executes `virtio_net_device_probe`, not the
  older `virtio_mmio_probe_contract` transcript.

## Evidence

```sh
TMPDIR=/private/tmp rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
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
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-18-40-+0200.xcresult
```

Harness checks:

```text
rtk git diff --check
rtk python3 -m unittest discover .codex/hooks/tests  # 32 OK
rtk python3 -m unittest discover .codex/rules/tests  # 5 OK
rtk python3 .codex/hooks/compact_plan_check.py       # exit 0; stale-history warning only
```

## Non-Claims

This checkpoint proves app-hosted execution of the Linux-owned virtio-net device
visibility probe. It does not prove packet I/O, carrier/link-up, DNS, NAT,
registry pull, OCI `netDevices`, or full OCI networking support.

## Next Work

Move from visibility to behavior in the Linux-owned virtio-net path:

- Prove the Linux netdev can transition through expected link state.
- Add packet-path proof only through standard Linux networking surfaces.
- Keep Darwin/iOS mechanics private to HostAdapter; do not expose custom ABI to
  Linux userspace.
