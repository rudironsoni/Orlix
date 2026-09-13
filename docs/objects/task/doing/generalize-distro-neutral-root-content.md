---
type: task
tags: [task, distributions, rootfs]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#235"
summary: "Import distro-neutral Linux ARM64 root content without changing guest executables."
task_of:
  - "[Deliver distro-neutral Linux ARM64 compatibility](../../story/doing/deliver-distro-neutral-linux-arm64-compatibility.md)"
depends_on:
  - "[Define Linux ARM64 ABI](define-linux-arm64-abi.md)"
blocks:
  - "[Boot unmodified Alpine ARM64](boot-unmodified-alpine-arm64.md)"
  - "[Boot unmodified Debian or Ubuntu ARM64](boot-unmodified-debian-or-ubuntu-arm64.md)"
---

# Generalize distro-neutral root content

Accept rootfs archives, extracted trees, and OCI-derived roots without rewriting ELF or loaders. Preserve or explicitly reject ownership, modes, links, device nodes, xattrs, capabilities, and whiteouts inside the selected OrlixInstance. Guest libraries stay Linux artifacts. The curated OrlixDistribution remains a default, not the ABI definition.
