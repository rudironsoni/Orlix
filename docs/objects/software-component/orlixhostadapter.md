---
type: software-component
tags:
  - architecture
  - ownership
updated: 2026-07-26
status: active
summary: "Private iOS and Darwin execution mechanics behind narrow Linux-facing boundaries."
part_of:
  - "[Orlix](../product/orlix.md)"
---

# OrlixHostAdapter

Private iOS and Darwin execution mechanics behind narrow Linux-facing boundaries. `OrlixHostAdapter` is linked as an implementation detail and is not a public framework, SDK, Linux runtime facade, or compatibility API.

Writable host shadows are subordinate to Linux-owned memory. Normal unmap may copy a live writable shadow back, but a postcommit refresh failure uses the distinct discard operation, which releases the stale host mapping without copying its contents over the already committed Linux page.

Its authoritative ownership boundaries are defined by [component ownership](../../concepts/component-ownership.md).
