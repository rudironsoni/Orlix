---
type: task
tags:
  - task
  - release
  - mobile-terminal
updated: 2026-07-15
status: todo
summary: "Archive, export, and upload the exact validated mobile terminal release candidate."
task_of:
  - "[Validate and publish the mobile terminal release](../../story/doing/validate-and-publish-mobile-terminal-release.md)"
depends_on:
  - "[Validate the mobile terminal simulator product](../doing/validate-mobile-terminal-simulator-product.md)"
  - "[Secure mobile terminal distribution approval](secure-mobile-terminal-distribution-approval.md)"
blocks:
  - "[Validate the mobile container simulator product](validate-mobile-container-simulator-product.md)"
---

# Archive, export, and upload the mobile terminal release

Archive and export only the candidate whose product identity matches the required proof tiers. Upload and App Store Connect operations require explicit operator authorization. An authorized physical-device gate, when selected, must pass for the same fingerprint before promotion.
