---
type: task
tags:
  - task
  - tssh
  - native-vendor
updated: 2026-08-29
status: doing
summary: "Establish reproducible native TSSH inputs for the supported Orlix app baseline."
task_of:
  - "[Deliver native TSSH transport](../../story/doing/deliver-native-tssh-transport.md)"
blocks:
  - "[Integrate the native TSSH terminal transport](../todo/integrate-native-tssh-terminal-transport.md)"
targets:
  - "[Orlix native app](../../software-component/orlix-native-app.md)"
applies:
  - "[Native TSSH security boundaries](../../../concepts/native-tssh-security-boundaries.md)"
---

# Establish the native TSSH foundation

Acquire and audit the release artifacts from the reviewed native TSSH Go fork
through the repository Make vendor interface. Record the source revision,
license, archive hashes, slice hashes, and deployment targets before the
framework enters the app graph.

Keep the existing iOS 16.1 app baseline. The native artifacts support that
baseline without changes to Orlix package dependencies.
