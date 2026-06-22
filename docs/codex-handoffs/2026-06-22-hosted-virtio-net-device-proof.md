# 2026-06-22 Handoff - Hosted Virtio-Net Device Proof

Active plan:

```text
docs/plans/active/oci-derived-environments-virtio-plane/
```

Checkpoint:

- `virtio_net_device_probe` is now wired as a dedicated hosted
  `OrlixKernelUpstreamTests` XCTest.
- The upstream test runner now isolates terminal identifiers by sanitized
  `kernelCommandLineSuffix`; this avoids replaying older `.kernel` kselftest
  transcript output.
- Focused hosted XCTest passed through the known-good iPhone 17 simulator.

Result bundle:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-18-40-+0200.xcresult
```

Do not expand this claim into full networking. The proof is still limited to
virtio-net device visibility through standard Linux surfaces:

- `/sys/bus/virtio/devices`
- `/sys/class/net`
- `RTM_GETLINK`
- `/proc/net/dev`

Continue with Linux-owned virtio-net behavior and packet-path proof. Keep the
Linux surface 100% Linux-compatible, with zero custom ABI and no host leakage.
