---
type: story
tags:
  - story
  - orlix-tcti
updated: 2026-07-15
status: doing
summary: "Prove safe TCTI execution from focused reducers through the app-hosted product path."
story_of:
  - "[Orlix TCTI](../../epic/doing/orlix-tcti.md)"
has_task:
  - "[Complete the pinned simulator TCTI ladder](../../task/doing/complete-pinned-simulator-tcti-ladder.md)"
  - "[Run authorized TCTI device validation](../../task/todo/run-authorized-tcti-device-validation.md)"
  - "[Promote TCTI as the product default](../../task/todo/promote-tcti-as-product-default.md)"
blocks:
  - "[Validate and publish the mobile terminal release](validate-and-publish-mobile-terminal-release.md)"
---

# Prove TCTI product execution

As an Orlix user, I want ordinary AArch64 Linux ELF instructions to execute safely through TCTI so Linux userspace reaches syscall dispatch and console output without executable guest mappings or a second process model.

The structured TCTI task envelope selects the current command and evidence. This story owns the durable order from reducers and kernel proof through the pinned simulator, authorized device validation, and product-default promotion.
