---
type: concept
tags:
  - architecture
  - security
  - tssh
updated: 2026-08-29
summary: "Native TSSH preserves SSH trust while adding bounded KCP, QUIC, forwarding, VPN, agent, resume, and Handoff boundaries."
applies:
  - "[Orlix](../objects/product/orlix.md)"
relates_to:
  - "[Native app and Herdr topology](native-app-and-herdr-topology.md)"
  - "[App Store host integration](app-store-host-integration.md)"
---

# Native TSSH security boundaries

## Overview

Orlix uses SSH to authenticate the user, verify the server host key, and start
`tsshd`. It then transfers terminal traffic to the pinned native TSSH KCP or
QUIC runtime. The native app owns this transport. Herdr continues to own the
Session, Workspace, Tab, and Pane hierarchy.

The existing SSH composition routes host-key candidates through
`KnownHostsManager` before authentication
([`Orlix/Orlix/Core/SSH/Client/SSHClientDependencies.swift:79`](../../Orlix/Orlix/Core/SSH/Client/SSHClientDependencies.swift)).
TSSH must use the same verified SSH client for bootstrap. It must not add an
independent trust-on-first-use store.

| Component | Authority and protected data | Enforcing control | Evidence |
| --- | --- | --- | --- |
| Server profile | Nonsecret host, port, transport, and policy | Versioned Codable model with safe defaults | [`Orlix/Orlix/Features/Servers/Domain/Server.swift:5`](../../Orlix/Orlix/Features/Servers/Domain/Server.swift) |
| SSH bootstrap | User authentication and server identity | Existing host-key verifier and approval flow | [`Orlix/Orlix/Core/SSH/Client/SSHClientDependencies.swift:4`](../../Orlix/Orlix/Core/SSH/Client/SSHClientDependencies.swift) |
| Native TSSH bridge | Opaque session, stream, and forwarding handles | Locked handle registry, bounded bootstrap decoder, explicit handle ownership | [`Orlix/Orlix/Features/TerminalSessions/Infrastructure/TSSH/TSSHCallGate.swift:34`](../../Orlix/Orlix/Features/TerminalSessions/Infrastructure/TSSH/TSSHCallGate.swift) |
| Resume storage | TSSH session secrets and nonsecret checkpoints | Device-only Keychain plus protected atomic files | [`Orlix/Orlix/Features/TerminalSessions/Infrastructure/TSSH/TSSHResumeStore.swift:39`](../../Orlix/Orlix/Features/TerminalSessions/Infrastructure/TSSH/TSSHResumeStore.swift) |
| Packet tunnel extension | VPN packets, routes, DNS, and TSSH credentials | Network Extension sandbox, opaque preference lookup key, shared device-only Keychain item | [`Orlix/Orlix/Features/TerminalSessions/Infrastructure/TSSH/TSSHVPNSecretStore.swift:18`](../../Orlix/Orlix/Features/TerminalSessions/Infrastructure/TSSH/TSSHVPNSecretStore.swift) |
| Herdr pane binding | Terminal bytes, resize events, and lifecycle | `TerminalTransportCoordinator` routes one transport to one pane | [`Orlix/Orlix/Features/TerminalSessions/Application/Transport/TerminalTransportCoordinator.swift:30`](../../Orlix/Orlix/Features/TerminalSessions/Application/Transport/TerminalTransportCoordinator.swift) |

## Threat model, trust boundaries, and assumptions

Protected assets are SSH and GPG private keys, SSH passwords, TSSH KCP and QUIC
credentials, resume session identifiers, VPN traffic and DNS, forwarded agent
authority, terminal scrollback, known-host trust, and Herdr pane ownership.

The important trust boundaries are:

- The untrusted network and remote `tsshd` cross the existing SSH host-key and
  authentication boundary before Orlix accepts native connection metadata.
- Go callbacks and values cross a native-language boundary. Swift validates
  sizes, enum values, ports, identifiers, certificates, and handle ownership.
- The app and packet-tunnel extension place only an opaque lookup UUID in VPN
  preferences. The complete TSSH configuration crosses through a device-only
  item in their shared Keychain group.
- A remote process can send SSH-agent requests when the server profile enables
  forwarding. The bridge exposes only the selected Orlix credential and never
  exports its private key.
- Local and remote forward listeners expose a new network entry point. Local
  forwards bind to loopback unless the user explicitly selects another bind
  address.
- [UNVERIFIED] Secure Handoff and GPG-agent forwarding are future task surfaces.
  They are not part of the current native transport implementation.

The attacker can control network packets, a malicious remote SSH account after
valid login, `tsshd` output, forwarded signing requests, listener connections,
and Handoff advertisements. The attacker does not start with the device
passcode, Keychain access, an approved SSH host key, the user's private keys, or
Network Extension signing authority.

Security objectives are to preserve SSH server identity, keep private material
out of CloudKit and VPN preferences, avoid secret logging, prevent handle and
pane confusion, bound all native and network input, require explicit authority
for signing and non-loopback exposure, and keep failure atomic during resume.

[UNVERIFIED] Packet-tunnel publication depends on Apple Network Extension
entitlement approval. [UNVERIFIED] Persistent non-VPN forwarding remains bound
by normal iOS background execution limits.

## Attack surface and mitigations

| Priority | Scenario and capability gain | Prerequisites | Existing or required control |
| --- | --- | --- | --- |
| Critical hypothesis | A forged bootstrap response supplies attacker transport keys and captures the terminal | Host-key verification bypass or parser confusion | Use the verified SSH channel, parse one bounded JSON object, validate every cryptographic field, and fail closed |
| High hypothesis | A remote process uses agent forwarding to sign with an unauthorized key | Agent forwarding enabled | Explicit per-server opt-in, selected identity only, bounded signing requests, no private-key export |
| High hypothesis | App-group compromise exposes VPN or resume credentials | Secret written to shared files | Store secrets only in device-bound shared Keychain items; keep app-group profiles nonsecret |
| High hypothesis | A malicious Handoff peer steals or terminates a live session | User accepts an advertised transfer | X25519 key agreement, HKDF-SHA256, ChaCha20-Poly1305, frame bounds, explicit receiver acceptance, and detach only after authenticated acknowledgement |
| Medium hypothesis | A forward unintentionally exposes a service to the local network or remote host | Forward rule created | Loopback default, explicit bind and remote-forward confirmation, validated ports, lifecycle cleanup |
| Medium hypothesis | Native callbacks use stale handles or reorder writes and resize events | Concurrent close, reconnect, or callback | Serialized handle registry, generation tokens, ordered writer, idempotent close, and stale-callback rejection |
| Medium hypothesis | Background output consumes memory or replays stale startup input | Suspended app or reconnect | Native discard notification before later bytes, pending input and output disabled by default, and no app-side replay after an unknown command result |
| Low hypothesis | Diagnostics disclose session keys, paths, or terminal content | Error or telemetry collection | Structured redaction tests and metadata-only health events |

These rows are implementation hypotheses, not confirmed vulnerabilities.

## Severity calibration

- Critical means remote private-key extraction, unauthenticated VPN compromise,
  or terminal interception without prior SSH account control.
- High means unauthorized signing, cross-device session theft, or secret
  disclosure that still needs an enabled integration or accepted transfer.
- Medium means unintended listener exposure, denial of service, stale-handle
  corruption, or bounded terminal-content disclosure.
- Low means metadata-only disclosure or a local self-only failure with no new
  authority.

Network Extension approval, explicit user configuration, biometric policy, and
existing SSH account control lower reachability. They do not remove the need for
the enforcing controls above.
