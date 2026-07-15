---
type: story
tags:
  - story
  - local-runtime
updated: 2026-07-15
status: doing
summary: "Deliver one upstream Linux runtime with a Default Local Instance and isolated namespaced Local Instances."
story_of:
  - "[Native app foundation](../../epic/doing/native-app-foundation.md)"
has_task:
  - "[Bind local sessions through OrlixOS](../../task/doing/bind-local-sessions-through-orlixos.md)"
  - "[Make app-hosted XCTest sessions terminate cleanly](../../task/done/make-app-hosted-xctest-sessions-terminate-cleanly.md)"
  - "[Implement namespaced Local Instance lifecycle](../../task/todo/implement-namespaced-local-instance-lifecycle.md)"
  - "[Prove concurrent Local Instance isolation](../../task/todo/prove-concurrent-local-instance-isolation.md)"
blocks:
  - "[Deliver OCI environment lifecycle](deliver-oci-environment-lifecycle.md)"
  - "[Validate and publish the mobile terminal release](validate-and-publish-mobile-terminal-release.md)"
---

# Deliver Local Runtime and Local Instances

As an Orlix user, I want one upstream Linux runtime to host a Default Local Instance and multiple persistent namespaced Local Instances so local systems have independent process, mount, hostname, network, root, and resource-policy identity without pretending to be separate kernels or virtual machines.

The story is complete when OrlixOS exposes the Local Runtime and Local Instance lifecycle, the Default Local Instance binds through the app-facing terminal session, and at least two Local Instances prove PID, mount, UTS, IPC, network, user, root, and cgroup isolation through normal Linux mechanisms.
