---
type: task
tags:
  - task
  - native-app-foundation
  - testing
updated: 2026-07-15
status: done
summary: "Make successful app-hosted runtime XCTest runs release their session resources and let xcodebuild exit cleanly."
task_of:
  - "[Deliver Local Runtime and Local Instances](../../story/doing/deliver-local-runtime-and-instances.md)"
---

# Make app-hosted XCTest sessions terminate cleanly

The app-hosted runtime proof now waits for terminal progress with XCTest expectations instead of synchronously polling a dispatch semaphore. The semaphore wait caused XCTest's runtime diagnostics to report a priority inversion; Xcode then launched asynchronous simulator diagnostics after the successful test, which kept `xcodebuild` in cleanup even though the test runner had exited successfully.

The XCTest-native wait preserves the bounded first-output and proof-completion deadlines without triggering that diagnostic path. No Linux driver, HostAdapter descriptor, output handler, terminal transport, or process-global kernel teardown change was required. The accepted Linux `console=` policy, source selection, multiplex transport, and terminal geometry behavior remain unchanged.
