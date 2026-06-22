# Handoff - Virtio-Net Device Probe

Date: 2026-06-22

## Summary

Added and packaged a Linux-owned `virtio_net_device_probe` kselftest overlay
program. The probe uses sysfs, rtnetlink, and procfs to prove virtio-net device
visibility as a Linux network interface when runtime execution is wired.

## Files

- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/virtio_net_device_probe.c`
- `OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/Makefile`
- `docs/plans/active/oci-derived-environments-virtio-plane/IMPLEMENT.md`
- `docs/plans/active/oci-derived-environments-virtio-plane/codex-handoffs/2026-06-22-handoff-virtio-net-device-probe.md`

## Evidence

```text
TMPDIR=/private/tmp rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
result=passed

Build/OrlixMLibC/kselftest/release/kselftest-list.txt:32:orlix:virtio_net_device_probe
Build/OrlixMLibC/test-initramfs/release/OrlixTestInitramfs.bundle/initramfs.list:39:file /orlix/virtio_net_device_probe ...
```

## Next Direction

Prove the new probe through a hosted OrlixOS terminal session without reusing
older `.kernel` session output. If runtime output shows missing netdev/sysfs or
rtnetlink visibility, route the fix to the Orlix virtio-net/MMIO backend.

