# 2026-06-22 Handoff - Virtio-Net Ethernet Surface

Active plan:

```text
docs/plans/active/oci-derived-environments-virtio-plane/
```

Checkpoint:

- `virtio_net_device_probe` now checks Ethernet-shaped Linux netdev properties.
- The probe remains Linux-surface only: sysfs plus rtnetlink.
- The hosted XCTest for `virtio_net_device_probe` passed on the known-good
  iPhone 17 simulator.

Result bundle:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-37-02-+0200.xcresult
```

Do not expand this into full networking. The current proof covers:

- virtio-net device discovery under `/sys/bus/virtio/devices`
- Linux netdev ownership under `/sys/class/net`
- Ethernet hardware type and 6-byte address length
- rtnetlink enumeration with nonzero 6-byte MAC
- `/proc/net/dev` visibility

Next work should target truthful link/carrier or packet-path behavior through
standard Linux networking surfaces only.
