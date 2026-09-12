---
type: story
tags:
  - story
  - release
  - container
updated: 2026-09-10
status: todo
summary: "Validate and publish the mobile container and Docker release after the mobile terminal release."
story_of:
  - "[Orlix release](../../epic/doing/orlix-release.md)"
has_task:
  - "[Secure downloaded-content distribution approval](../../task/todo/secure-downloaded-content-distribution-approval.md)"
  - "[Validate the mobile container simulator product](../../task/todo/validate-mobile-container-simulator-product.md)"
  - "[Validate an authorized mobile container device](../../task/todo/validate-authorized-mobile-container-device.md)"
  - "[Archive, export, and upload the mobile container release](../../task/todo/archive-export-and-upload-mobile-container-release.md)"
depends_on:
  - "[Validate and publish the mobile terminal release](../doing/validate-and-publish-mobile-terminal-release.md)"
  - "[Deliver OCI environment lifecycle](../doing/deliver-oci-environment-lifecycle.md)"
blocks:
  - "[Validate and publish the native macOS release](validate-and-publish-native-macos-release.md)"
---

# Validate and publish the mobile container release

As a release operator, I want the already-published mobile terminal product to add OrlixContainer, Docker, and Compose behavior only after OrlixInstance isolation, OCI lifecycle, Docker conformance, and downloaded-content distribution approval pass for one exact candidate.

The container release cannot replace or precede the mobile terminal release. Simulator validation is mandatory, physical-device validation is authorization-dependent, and archive or upload requires explicit operator authorization.
