---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-26
status: accepted
external_id: "ADR-0026"
summary: "Host persistent namespaced OrlixMachine systems inside one upstream Linux kernel."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# ADR 0026: Use One Kernel With Namespaced Orlix Machines

## Status

Accepted.

## Context

OrlixKernel and its host adapter have process-global boot and runtime state. Multiple reentrant kernels in one app process are not the product design. Orlix still needs multiple persistent Linux systems, per-project isolation, containers, independent lifecycle, and resource management without calling those systems kernels or VMs.

## Decision

One upstream OrlixKernel hosts multiple `OrlixMachine` values as persistent, namespaced Linux userspace systems. `OrlixMachine` is the sole public local Linux lifecycle name. Retired Local Runtime and Local Instance names receive no type, symbol, module, target, or documentation compatibility aliases.

Each OrlixMachine owns an init process, root and state, PID namespace, mount namespace, UTS namespace, IPC namespace, network namespace, user namespace, and cgroup v2 subtree. Machines share the kernel while keeping process, mount, hostname, network, root, and resource-policy identity separate.

A normal Linux userspace supervisor manages OrlixMachine lifecycle through Linux-shaped control mechanisms. The default machine starts on demand. New machines are stopped by default, and autostart is explicit.

Containers are OCI application workloads assigned to exactly one OrlixMachine through `OrlixOS.Containers`. A container cannot exist without a machine, and the machine-local Docker-compatible socket controls only that machine's containers.

Upstream Linux owns namespace, cgroup, process, signal, mount, VFS, socket, and networking semantics. OrlixOS owns the app-facing OrlixMachine lifecycle API, distribution assembly, and `OrlixOS.Containers` control-plane integration. OrlixHostAdapter remains private Apple and Darwin mechanics and does not acquire lifecycle policy.

## Consequences

User-facing language uses OrlixMachine and Container. An OrlixMachine is not called a kernel, VM, environment, Local Runtime, or Local Instance.

The full machine release must prove at least two concurrent OrlixMachine values with isolated PID, mount, hostname, network, root, and cgroup state through the OrlixOS session surface. It must also prove the ADR 0017 runtime ladder, including PTYs, shell, dynamic loader, networking, persistence, jq, curl, and zsh.
