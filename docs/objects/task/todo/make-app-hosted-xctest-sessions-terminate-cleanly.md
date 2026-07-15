---
type: task
tags:
  - task
  - native-app-foundation
  - testing
updated: 2026-07-15
status: todo
summary: "Make successful app-hosted runtime XCTest runs release their session resources and let xcodebuild exit cleanly."
task_of:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
---

# Make app-hosted XCTest sessions terminate cleanly

Determine why an app-hosted runtime XCTest can report success while `xcodebuild` remains in test-session cleanup. Distinguish Orlix lifecycle ownership from XCTest or simulator behavior by checking recurring Linux driver work, HostAdapter pipe handlers and registered output file descriptors, the process-global booted kernel and session lifetime, and the surrounding test-runner cleanup path.

The task is complete when a focused app-hosted runtime test reports success and its `xcodebuild` command exits successfully without manual interruption, with no leaked output registration, readability handler, or avoidable recurring work. The accepted Linux `console=` policy, source selection, multiplex transport, and terminal geometry behavior are regression constraints and are not reopened by this task.
