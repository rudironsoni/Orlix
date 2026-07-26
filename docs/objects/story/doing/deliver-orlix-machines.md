---
type: story
tags:
  - story
  - orlix-machine
updated: 2026-07-26
status: doing
summary: "Deliver persistent namespaced OrlixMachine systems through the sole public OrlixOS SDK."
story_of:
  - "[Native app foundation](../../epic/doing/native-app-foundation.md)"
has_task:
  - "[Bind OrlixMachine sessions through OrlixOS](../../task/doing/bind-orlix-machine-sessions-through-orlixos.md)"
  - "[Make app-hosted XCTest sessions terminate cleanly](../../task/done/make-app-hosted-xctest-sessions-terminate-cleanly.md)"
  - "[Implement namespaced OrlixMachine lifecycle](../../task/todo/implement-namespaced-orlix-machine-lifecycle.md)"
  - "[Prove concurrent OrlixMachine isolation](../../task/todo/prove-concurrent-orlix-machine-isolation.md)"
blocks:
  - "[Deliver OCI environment lifecycle](deliver-oci-environment-lifecycle.md)"
  - "[Validate and publish the mobile terminal release](validate-and-publish-mobile-terminal-release.md)"
---

# Deliver Orlix machines

As an Orlix user, I want the sole public OrlixOS SDK to host persistent `OrlixMachine` systems inside one upstream Linux kernel so local systems have independent process, mount, hostname, network, root, and resource-policy identity without pretending to be separate kernels or virtual machines.

The story is complete when OrlixOS exposes OrlixMachine lifecycle, the default machine binds through the app-facing terminal session, and at least two machines prove PID, mount, UTS, IPC, network, user, root, and cgroup isolation through normal Linux mechanisms. The renamed API and work pages are architecture alignment only; the lifecycle and isolation tasks remain unfinished.
