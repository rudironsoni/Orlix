---
type: story
tags: [story, bazel, apple]
updated: 2026-09-06
status: done
summary: "Declare all private Apple layers, OrlixOS, the app, extensions, resources, tests, and Xcode project surfaces in Bazel."
story_of:
  - "[Adopt Bazel product graph](../../epic/done/adopt-bazel-product-graph.md)"
has_task:
  - "[Implement the Bazel Apple product graph](../../task/done/implement-bazel-apple-product-graph.md)"
---

# Move The Apple Product Graph To Bazel

As an Orlix developer, I want Bazel to own Apple compilation, linking, resources, signing inputs, tests, and local project generation without exposing private implementation products.
