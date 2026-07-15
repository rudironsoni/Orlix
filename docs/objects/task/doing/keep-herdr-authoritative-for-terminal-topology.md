---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-15
status: doing
summary: "Use Herdr as the sole owner of terminal workspaces, tabs, panes, focus, and topology."
task_of:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
depends_on:
  - "[Integrate and validate the Herdr terminal platform](../todo/integrate-and-validate-herdr-terminal-platform.md)"
blocks:
  - "[Validate mobile platform presentation](../todo/validate-mobile-platform-presentation.md)"
  - "[Validate the mobile terminal simulator product](validate-mobile-terminal-simulator-product.md)"
---

# Keep Herdr authoritative for terminal topology

Keep feature state and shared terminal UI independent from platform presentation while platform adapters consume the same real Herdr topology. Focused app and UI tests must prove that no parallel topology model is introduced. Implementation may proceed against the approved contract while delivery remains blocked on the commercially approved, validated Herdr platform.
