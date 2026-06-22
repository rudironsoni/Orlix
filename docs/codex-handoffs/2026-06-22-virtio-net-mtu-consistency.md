# 2026-06-22 Handoff - Virtio-Net MTU Consistency

Active plan:

```text
docs/plans/active/oci-derived-environments-virtio-plane/
```

Checkpoint:

- `virtio_net_device_probe` now checks MTU consistency across sysfs and
  rtnetlink.
- Hosted XCTest passed on the known-good iPhone 17 simulator.
- The proof remains Linux-surface only and introduces no custom ABI.

Result bundle:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-46-35-+0200.xcresult
```

Current proven surface:

- virtio-net device discovery under `/sys/bus/virtio/devices`
- Linux netdev ownership under `/sys/class/net`
- Ethernet hardware type and 6-byte address length
- rtnetlink enumeration with nonzero 6-byte MAC
- MTU consistency across `/sys/class/net/<ifname>/mtu` and `IFLA_MTU`
- `/proc/net/dev` visibility

Do not claim packet I/O, carrier/link-up, DNS, NAT, registry pull, OCI
`netDevices`, or full OCI networking from this checkpoint.
