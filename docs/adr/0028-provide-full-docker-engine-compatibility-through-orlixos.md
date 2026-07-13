# ADR 0028: Provide Full Docker Engine Compatibility Through OrlixOS

## Status

Accepted.

## Context

Orlix needs OrbStack-like Docker compatibility while preserving OrlixKernel as upstream Linux and OrlixOS as the Kit. Running `dockerd`, `containerd`, or `runc` as the product runtime would create a competing lifecycle and control plane. Parsing Docker commands in Swift or moving Engine policy into OrlixHostAdapter or the kernel would violate existing ownership.

The requested compatibility target is the complete Linux Docker Engine surface. Windows Containers are not needed, but Swarm, services, configs, secrets, plugins, BuildKit, Buildx, Compose, contexts, versioned APIs, networking, volumes, builds, exec, logs, stats, events, and privileged Linux Containers remain in scope.

## Decision

Provide a normal Linux userspace Docker-compatible service inside each Local Instance, backed by OrlixOS's OCI control plane. Do not use `dockerd`, `containerd`, or `runc` as the Orlix runtime. Do not parse Docker CLI commands in Swift, and do not place Docker behavior in OrlixKernel or OrlixHostAdapter.

Target Docker Engine 29.6.1 behavior. Advertise Engine API 1.55, default the minimum accepted API to 1.40, and permit configured backward compatibility through API 1.24. Use pinned upstream Docker CLI, Compose, Buildx, BuildKit, Engine API schemas, and applicable pristine integration tests as compatibility oracles.

The service implements Containers, images, registries, volumes, bind mounts, networks, builds, cache import/export, logs, exec, stats, events, health, restart policies, Compose, contexts, Swarm, services, configs, secrets, and daemon-plugin-compatible behavior. BuildKit uses an Orlix executor and snapshot backend that runs build steps through the Orlix OCI control plane.

Docker `--privileged` is supported inside a Local Instance according to Linux namespace semantics. It does not grant macOS host root. Windows image manifests and Windows Container creation fail with Docker-compatible unsupported-platform errors. No other Engine area is deliberately removed.

Compose follows upstream reconciliation behavior. Projects bind persistently to an existing Local Instance or create a dedicated Local Instance according to the app-wide default and an optional project override. Unsupported fields fail explicitly and are never silently discarded.

The default macOS Docker context is `orlix` and targets the Default Local Instance. Additional Local Instances may expose `orlix-<instance-slug>` contexts. Existing Docker binaries and contexts are preserved, and Orlix does not change the active context without explicit user action.

## Consequences

Real virtio-net, network namespaces, veth and bridge behavior, DNS, outbound connectivity, service discovery, published ports, and Swarm traffic are release gates. UI-only resource records cannot masquerade as Docker-compatible runtime behavior.

Windows Container tests are the only product-scope exclusions in the Docker conformance matrix. Other failures and skips remain visible and cannot be reinterpreted as full compatibility.
