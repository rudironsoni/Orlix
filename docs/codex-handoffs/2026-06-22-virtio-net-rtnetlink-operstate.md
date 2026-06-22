# 2026-06-22 Handoff - Virtio-Net Rtnetlink Operstate

Active plan:

```text
docs/plans/active/oci-derived-environments-virtio-plane/
```

Checkpoint:

- `virtio_net_device_probe` now checks standard rtnetlink `IFLA_OPERSTATE`.
- It also keeps the prior MTU consistency proof across sysfs and rtnetlink.
- Hosted XCTest passed on the known-good iPhone 17 simulator.

Important negative evidence:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-54-26-+0200.xcresult
not ok 7 - virtio-net netdev reports a standard operstate through sysfs
```

Do not claim sysfs operstate from this checkpoint.

Green result bundle:

```text
/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_18-59-02-+0200.xcresult
```

Current proven surface:

- virtio-net device discovery under `/sys/bus/virtio/devices`
- Linux netdev ownership under `/sys/class/net`
- Ethernet hardware type and 6-byte address length
- rtnetlink enumeration with nonzero 6-byte MAC
- MTU consistency across `/sys/class/net/<ifname>/mtu` and `IFLA_MTU`
- standard rtnetlink `IFLA_OPERSTATE`
- `/proc/net/dev` visibility

Do not claim carrier/link-up, packet I/O, DNS, NAT, registry pull, OCI
`netDevices`, or full OCI networking from this checkpoint.
