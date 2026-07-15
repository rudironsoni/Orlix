---
type: task
tags:
  - task
  - release
  - macos
updated: 2026-07-15
status: todo
summary: "Implement the native Apple-silicon macOS product after the mobile container release."
task_of:
  - "[Validate and publish the native macOS release](../../story/todo/validate-and-publish-native-macos-release.md)"
depends_on:
  - "[Archive, export, and upload the mobile container release](archive-export-and-upload-mobile-container-release.md)"
blocks:
  - "[Validate the native macOS product](validate-native-macos-product.md)"
---

# Implement the native macOS product

Add the native Apple-silicon target, required framework slices, user-scoped runtime service, embedded helpers, Herdr integration, Docker contexts, and shared-folder grants without creating a separate runtime architecture from the proven mobile product.
