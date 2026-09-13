---
type: task
tags: [task, ios, ipados]
updated: 2026-09-13
status: doing
external_id: "github:rudironsoni/Orlix#50"
summary: "Complete the native Orlix iOS and iPadOS application foundation."
task_of:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
targets:
  - "[Orlix native app](../../software-component/orlix-native-app.md)"
  - "[OrlixKit](../../software-component/orlixkit.md)"
---

# Complete the iOS and iPadOS application foundation

Preserve the imported application, product identity, resources, provenance, notices, tests, entitlements, privacy declarations, StoreKit, and native dependencies. Orlix.app remains the composition root and consumes Linux lifecycle only through OrlixKit. App-only builds can omit local Linux without adding a second domain model. Record exact iPhone and iPad destinations, failures, skips, and result bundles.
