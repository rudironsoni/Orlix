---
type: story
tags:
  - story
  - native-app-foundation
  - tssh
updated: 2026-08-29
status: doing
summary: "Deliver TSSH as a native resumable terminal transport with forwarding, VPN, agents, and secure device transfer."
story_of:
  - "[Native app foundation](../../epic/doing/native-app-foundation.md)"
has_task:
  - "[Establish the native TSSH foundation](../../task/doing/establish-native-tssh-foundation.md)"
  - "[Integrate the native TSSH terminal transport](../../task/todo/integrate-native-tssh-terminal-transport.md)"
  - "[Deliver TSSH forwarding VPN agents and transfer](../../task/todo/deliver-tssh-forwarding-vpn-agents-and-transfer.md)"
---

# Deliver native TSSH transport

As an Orlix user, I want TSSH as a native KCP or QUIC transport so terminal
sessions roam, resume, forward traffic, and transfer between my devices without
running the TSSH command-line client inside a shell.

The story is complete when supported iOS and iPadOS releases can create and resume a
TSSH terminal through the native app transport boundary, all configured
forwarding and agent surfaces enforce the native TSSH security model, the
approved packet-tunnel extension passes its interoperability gate, encrypted
device transfer preserves Herdr pane ownership, and the repository Make gates
pass. Network Extension distribution still requires Apple's external approval.
