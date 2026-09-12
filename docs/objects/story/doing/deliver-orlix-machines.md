---
type: story
tags:
  - story
  - orlix-instance
updated: 2026-09-10
status: doing
summary: "Deliver persistent namespaced OrlixInstance systems through the public OrlixKit SDK."
story_of:
  - "[Native app foundation](../../epic/doing/native-app-foundation.md)"
has_task:
  - "[Bind OrlixInstance sessions through OrlixOS](../../task/doing/bind-orlix-machine-sessions-through-orlixos.md)"
  - "[Make app-hosted XCTest sessions terminate cleanly](../../task/done/make-app-hosted-xctest-sessions-terminate-cleanly.md)"
  - "[Implement namespaced OrlixInstance lifecycle](../../task/todo/implement-namespaced-orlix-machine-lifecycle.md)"
  - "[Prove concurrent OrlixInstance isolation](../../task/todo/prove-concurrent-orlix-machine-isolation.md)"
blocks:
  - "[Deliver OCI environment lifecycle](deliver-oci-environment-lifecycle.md)"
  - "[Validate and publish the mobile terminal release](validate-and-publish-mobile-terminal-release.md)"
---

# Deliver Orlix machines

As an Orlix user, I want OrlixKit to host persistent `OrlixInstance` systems inside one upstream Linux kernel so local systems have independent process, mount, UTS/hostname, network-namespace, user/credential, IPC, root, and cgroup identity without pretending to be separate kernels or virtual machines.

The story is complete when OrlixKit exposes OrlixInstance lifecycle, the default instance binds through the app-facing terminal session, and at least two instances prove PID, mount, UTS, IPC, network-namespace, user, root, and cgroup isolation through normal Linux mechanisms. The renamed API and work pages are architecture alignment only; the lifecycle and isolation tasks remain unfinished.
