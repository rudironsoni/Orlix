---
type: architecture-decision
tags:
  - architecture
  - decision
updated: 2026-07-26
status: accepted
external_id: "ADR-0027"
summary: "Durable Orlix architecture decision ADR 0027."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# ADR 0027: Use App-Store-Only Cross-Platform Host Integration

## Status

Accepted.

## Context

Orlix targets iOS, iPadOS, and native Apple-silicon macOS while preserving one Linux userspace contract. The Mac app needs a persistent user-scoped runtime, embedded CLIs, local Herdr execution, Docker contexts, shared folders, and external-shell access. App Store sandboxing does not permit treating a helper as a general privilege escape, and Apple's embedded-helper documentation directly proves only app-launched helper execution.

ADR 0023 keeps release executable content conservative until an App Store-safe channel is explicitly designed and reviewed. Herdr marketplace content, downloaded Linux packages, OCI images, and Docker plugins make that review a product gate.

## Decision

Use the App Store as the only distribution channel on every supported platform. Minimum versions are iOS and iPadOS 16.1 and, when Mac work begins, macOS 13.3. The future Mac target is a native macOS app and is Apple-silicon-only.

Publish the full terminal, remote transport, commercially approved Herdr surface, and OrlixMachine on iOS and iPadOS first. Publish the mobile `OrlixOS.Containers`, Docker, and Compose release second. Begin the native macOS product only after both mobile releases are in good shape and published to the App Store. There is no public remote-only Mac phase: the eventual Mac release follows the same terminal and container product contract.

Mobile implementation must preserve the native application's macOS-compatible source, resources, package declarations, and conditional compilation and must avoid unnecessary UIKit-only assumptions in shared feature code. This is foundation work only. It does not authorize early implementation of the Mac target, Mac OrlixKernel slice, runtime service, helper tools, external Herdr CLI, or Docker contexts.

On macOS, embed normal CLI and helper executables under `Orlix.app/Contents/MacOS` using an `Embed Helper Tools` copy phase, Code Sign On Copy, Hardened Runtime, `SKIP_INSTALL=YES`, and disabled `CODE_SIGN_INJECT_BASE_ENTITLEMENTS`. App-launched helpers use App Sandbox and sandbox inheritance as Apple documents. Exported App Store packages are inspected for identifier, architecture, signature, runtime flags, and entitlements.

Use `SMAppService` for an approved user-scoped runtime service with explicit user approval. App, runtime, and approved CLI clients communicate through user-scoped sockets in `group.com.rudironsoni.orlix`. External-shell invocation is a separate acceptance gate because sandbox inheritance does not prove that launch path.

The eventual macOS Docker release is rootless. Host bind mounts require explicit shared-folder grants and security-scoped bookmarks. Host ports are unprivileged by default. Root inside an Orlix Linux namespace does not imply macOS host root.

Host low ports, unrestricted host paths, host-root services, arbitrary external `ssh-agent` and 1Password sockets, and other truly unsupported host capabilities are deferred to a later last-resort helper or broker research sprint. That work may ship only through an App Store-compliant mechanism and must not delay or weaken the rootless release.

Downloaded executable content, Herdr marketplace behavior, OCI images, and Docker plugins remain blocked from public distribution until legal review, entitlement review, and App Review establish an acceptable release path. If the required full surface cannot be approved, the affected public release stops.

This decision extends ADR 0023. It does not supersede or weaken ADR 0023's conservative release policy for downloaded executable content.

## Consequences

Orlix has no standalone or alternate Mac distribution that silently carries broader privileges. Capability differences are reported honestly and unavailable features are not advertised.

Apple Container and Virtualization.framework may be used as Mac-only references or oracles where useful, but they do not become the Orlix runtime on iOS, iPadOS, or macOS.
