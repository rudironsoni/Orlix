---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-15
status: todo
summary: "Prove concurrent Local Instances preserve independent lifecycle, terminal, and Linux-visible state."
task_of:
  - "[Deliver Local Runtime and Local Instances](../../story/doing/deliver-local-runtime-and-instances.md)"
depends_on:
  - "[Implement namespaced Local Instance lifecycle](implement-namespaced-local-instance-lifecycle.md)"
blocks:
  - "[Implement OCI runtime lifecycle](implement-oci-runtime-lifecycle.md)"
  - "[Validate the mobile terminal simulator product](../doing/validate-mobile-terminal-simulator-product.md)"
---

# Prove concurrent Local Instance isolation

Demonstrate that concurrent instances preserve independent session lifetime, terminal attachment, namespace-visible state, and failure behavior. This proof is required before OCI lifecycle or the mobile terminal product can claim instance isolation.
