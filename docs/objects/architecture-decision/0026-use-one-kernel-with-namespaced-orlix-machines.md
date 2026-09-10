---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-09-10
status: accepted
external_id: "ADR-0026"
summary: "Host persistent namespaced OrlixInstance systems inside one upstream Linux kernel."
part_of:
  - "[Orlix](../product/orlix.md)"
amended_by:
  - "[ADR 0040](0040-recover-orlixkit-product-boundaries-and-build-reuse.md)"
---

# ADR 0026: Use One Kernel With Namespaced Orlix Instances

## Status

Accepted.

## Context

OrlixKernel and its host adapter have process-global boot and runtime state. Multiple reentrant kernels in one app process are not the product design. Orlix still needs multiple persistent Linux systems, per-project isolation, containers, independent lifecycle, and resource management without calling those systems kernels or VMs.

## Decision

One upstream Linux kernel hosted by OrlixOS hosts multiple `OrlixInstance` values as persistent, namespaced Linux userspace systems. `OrlixInstance` is the public local Linux lifecycle name. Retired Local Runtime and OrlixMachine names receive no new compatibility aliases; existing source identifiers are migration work until the API graph is changed.

Each OrlixInstance owns an init process, root and state, PID namespace, mount namespace, UTS namespace, IPC namespace, network namespace, user namespace, and cgroup v2 subtree. Instances share the kernel while keeping process, mount, hostname, network-namespace identity and namespace-local state, root, credentials, IPC, and resource-policy identity separate.

A normal Linux userspace supervisor manages OrlixInstance lifecycle through Linux-shaped control mechanisms. The default instance starts on demand. New instances are stopped by default, and autostart is explicit.

OrlixContainer workloads are OCI application workloads assigned to exactly one OrlixInstance through OrlixKit. A container cannot exist without an instance, and the instance-local Docker-compatible socket controls only that instance's containers.

Upstream Linux owns namespace, cgroup, process, signal, mount, VFS, socket, and networking semantics. OrlixKit owns the app-facing OrlixInstance and OrlixContainer API boundary, OrlixOS owns the running OS, and OrlixDistribution owns guest assembly. OrlixHostAdapter remains private Apple and Darwin mechanics and does not acquire lifecycle policy.

## Consequences

User-facing language uses OrlixInstance and OrlixContainer. An OrlixInstance is not called a kernel, VM, environment, Local Runtime, or Local Instance.

The full instance release must prove at least two concurrent OrlixInstance values with isolated PID, mount, UTS/hostname, network namespace identity and namespace-local state, user/credential, IPC, root, and cgroup state through OrlixKit. Functional external networking is claimed only to the proof tier established by the current virtio/network implementation. It must also prove the ADR 0017 runtime ladder, including PTYs, shell, dynamic loader, persistence, jq, curl, and zsh.
