---
type: task
tags:
  - task
  - tssh
  - forwarding
  - vpn
updated: 2026-08-29
status: todo
summary: "Deliver TSSH forwarding, VPN, agent bridges, headless sessions, and encrypted device transfer."
task_of:
  - "[Deliver native TSSH transport](../../story/doing/deliver-native-tssh-transport.md)"
depends_on:
  - "[Integrate the native TSSH terminal transport](integrate-native-tssh-terminal-transport.md)"
targets:
  - "[Orlix native app](../../software-component/orlix-native-app.md)"
applies:
  - "[Native TSSH security boundaries](../../../concepts/native-tssh-security-boundaries.md)"
---

# Deliver TSSH forwarding VPN agents and transfer

Add TCP local and remote forwarding, TCP SOCKS forwarding, headless
TSSH sessions, SSH and GPG agent bridges, the approved packet-tunnel extension,
and encrypted `NSUserActivity` device transfer. Secrets remain in device-bound
Keychain storage. App-group files contain only validated nonsecret profiles.

The task requires native interoperability tests and Network Extension runtime
evidence. Apple entitlement approval remains an external distribution gate.
