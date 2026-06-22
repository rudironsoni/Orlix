# Network RTM_GETLINK Checkpoint

Date: 2026-06-22

Active plan:

`docs/plans/active/oci-derived-environments-virtio-plane/`

Current checkpoint:

- Extended Linux-owned `network_namespace_probe`.
- The probe now performs an `RTM_GETLINK` dump over `NETLINK_ROUTE`.
- The probe requires the Linux loopback interface to appear in `RTM_NEWLINK`
  output with positive interface index and `IFLA_IFNAME == "lo"`.
- The app-hosted kernel XCTest asserts the new marker.

Validation:

```text
TMPDIR=/private/tmp rtk make -f OrlixKernel/Makefile kselftest PROFILE=release
result: passed

/Volumes/1TB/Xcode/DerivedData/Logs/Test/Test-OrlixKernelUpstreamTests-2026.06.22_16-50-31-+0200.xcresult
result=Passed
passedTests=1
failedTests=0
skippedTests=0
totalTestCount=1
expectedFailures=0
```

Non-claims:

- This proves rtnetlink loopback enumeration only.
- It does not claim full OCI networking, virtio-net packet I/O, registry pull,
  product `orlix run`, complete namespace/cgroup integration, or full OCI
  Runtime compliance.
