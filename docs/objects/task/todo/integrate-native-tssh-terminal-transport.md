---
type: task
tags:
  - task
  - tssh
  - terminal-transport
updated: 2026-08-29
status: todo
summary: "Integrate native TSSH bootstrap, shell, roaming, resume, transfer, and Herdr pane lifecycle."
task_of:
  - "[Deliver native TSSH transport](../../story/doing/deliver-native-tssh-transport.md)"
depends_on:
  - "[Establish the native TSSH foundation](../doing/establish-native-tssh-foundation.md)"
blocks:
  - "[Deliver TSSH forwarding VPN agents and transfer](deliver-tssh-forwarding-vpn-agents-and-transfer.md)"
targets:
  - "[Orlix native app](../../software-component/orlix-native-app.md)"
applies:
  - "[Native TSSH security boundaries](../../../concepts/native-tssh-security-boundaries.md)"
---

# Integrate the native TSSH terminal transport

Add TSSH beside SSH, Mosh, and Eternal Terminal. Bootstrap `tsshd` through the
existing verified SSH path, parse one bounded server response, and transfer
shell, command, resize, file-transfer, health, reconnect, and resume lifecycle
to the pinned native KCP or QUIC runtime.

Bind each runtime to the existing terminal transport coordinator. Herdr remains
the sole owner of Session, Workspace, Tab, Pane, focus, and topology.
