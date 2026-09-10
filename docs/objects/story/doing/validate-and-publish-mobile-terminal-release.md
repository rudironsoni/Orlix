---
type: story
tags:
  - story
  - release
  - mobile-terminal
updated: 2026-09-10
status: doing
summary: "Validate and publish the first iOS and iPadOS release with the full terminal, Herdr, and OrlixInstance contract."
story_of:
  - "[Orlix release](../../epic/doing/orlix-release.md)"
has_task:
  - "[Secure mobile terminal distribution approval](../../task/todo/secure-mobile-terminal-distribution-approval.md)"
  - "[Validate the mobile terminal simulator product](../../task/doing/validate-mobile-terminal-simulator-product.md)"
  - "[Validate an authorized mobile terminal device](../../task/todo/validate-authorized-mobile-terminal-device.md)"
  - "[Archive, export, and upload the mobile terminal release](../../task/todo/archive-export-and-upload-mobile-terminal-release.md)"
depends_on:
  - "[Establish native application foundation](establish-native-application-foundation.md)"
  - "[Deliver Orlix machines](deliver-orlix-machines.md)"
  - "[Prove TCTI product execution](prove-tcti-product-execution.md)"
blocks:
  - "[Validate and publish the mobile container release](../todo/validate-and-publish-mobile-container-release.md)"
---

# Validate and publish the mobile terminal release

As a release operator, I want one iOS and iPadOS product identity to advance through the terminal, Herdr, OrlixInstance, OrlixTCTI, simulator, signing, and distribution gates so the first public mobile release matches the evidence used to authorize it.

Simulator validation is mandatory. Physical-device validation requires explicit authorization and becomes a promotion dependency only when selected for the candidate. Archive and upload require passing public-distribution approval and explicit operator authorization.
