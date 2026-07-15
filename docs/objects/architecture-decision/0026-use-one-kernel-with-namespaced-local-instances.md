---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-15
status: accepted
external_id: "ADR-0026"
summary: "Durable Orlix architecture decision ADR 0026."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# ADR 0026: Use One Kernel With Namespaced Local Instances

## Status

Accepted.

## Context

OrlixKernel and its host adapter have process-global boot and runtime state. Multiple reentrant kernels in one app process are not the product design. Orlix still needs multiple persistent Linux systems, per-project isolation, containers, independent lifecycle, and resource management without calling those systems kernels or VMs.

## Decision

One Local Runtime contains one upstream OrlixKernel. Multiple Local Instances run concurrently inside that kernel as persistent, namespaced Linux userspace systems.

Each Local Instance owns an init process, root and state, PID namespace, mount namespace, UTS namespace, IPC namespace, network namespace, user namespace, and cgroup v2 subtree. Instances share the kernel while keeping process, mount, hostname, network, root, and resource-policy identity separate.

A normal Linux userspace supervisor manages Local Instance lifecycle through Linux-shaped control mechanisms. The Default Local Instance starts on demand. New Local Instances are stopped by default, and autostart is explicit.

Containers are OCI application workloads assigned to exactly one Local Instance. A Container cannot exist without a Local Instance, and the instance-local Docker-compatible socket controls only that instance's Containers.

Upstream Linux owns namespace, cgroup, process, signal, mount, VFS, socket, and networking semantics. OrlixOS owns the app-facing Local Runtime and Local Instance lifecycle API, distribution assembly, and OCI control-plane integration. OrlixHostAdapter remains private Apple and Darwin mechanics and does not acquire lifecycle policy.

## Consequences

User-facing language uses Local Runtime, Local Instance, and Container. A Local Instance is not called a kernel, VM, environment, or Container.

The full Local Runtime release must prove at least two concurrent Local Instances with isolated PID, mount, hostname, network, root, and cgroup state through the OrlixOS session surface. It must also prove the ADR 0017 runtime ladder, including PTYs, shell, dynamic loader, networking, persistence, jq, curl, and zsh.
