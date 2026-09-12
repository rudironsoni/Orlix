---
type: story
tags:
  - story
  - release
  - macos
updated: 2026-09-10
status: todo
summary: "Implement, validate, and publish the native Apple-silicon macOS product after both mobile releases."
story_of:
  - "[Orlix release](../../epic/doing/orlix-release.md)"
has_task:
  - "[Implement the native macOS product](../../task/todo/implement-native-macos-product.md)"
  - "[Validate the native macOS product](../../task/todo/validate-native-macos-product.md)"
  - "[Archive, export, and upload the native macOS release](../../task/todo/archive-export-and-upload-native-macos-release.md)"
depends_on:
  - "[Validate and publish the mobile container release](validate-and-publish-mobile-container-release.md)"
---

# Validate and publish the native macOS release

As a release operator, I want the native Apple-silicon macOS product to begin only after the mobile terminal and mobile container releases are published so the Mac app carries the same terminal, OrlixInstance, OrlixContainer, and App Store contract instead of creating an early divergent product.

The story includes the native target, the public OrlixKit slice and its private native implementation and guest distribution artifacts, user-scoped runtime service, embedded helpers and CLIs, Herdr integration, Docker contexts, shared-folder grants, product validation, and authorized Mac App Store publication.
