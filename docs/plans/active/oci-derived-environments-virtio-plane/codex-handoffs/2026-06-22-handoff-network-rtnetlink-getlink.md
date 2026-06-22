# Handoff - Network RTM_GETLINK Proof

Date: 2026-06-22

Current checkpoint:

- Extended `network_namespace_probe` to send an `RTM_GETLINK` request over
  `NETLINK_ROUTE`.
- The probe parses `RTM_NEWLINK` messages and requires the Linux loopback
  interface to appear with positive interface index and `IFLA_IFNAME == "lo"`.
- Updated `OrlixKernelUpstreamTests` to require the
  `RTM_GETLINK reports loopback interface` output marker.

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

Harness:

```text
rtk git diff --check
rtk python3 -m unittest discover .codex/hooks/tests
rtk python3 -m unittest discover .codex/rules/tests
rtk python3 .codex/hooks/compact_plan_check.py
```

The compact-plan check exited `0` with the known stale-history/current-status
warnings.

Non-claims:

- This proves rtnetlink loopback enumeration only.
- It does not claim full OCI networking, virtio-net packet I/O, registry pull,
  product `orlix run`, complete namespace/cgroup integration, or full OCI
  Runtime compliance.
