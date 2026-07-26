---
type: task
tags:
  - task
  - native-app-foundation
updated: 2026-07-26
status: doing
summary: "Keep the Session to Workspace to Tab to Pane hierarchy owned solely by Herdr."
task_of:
  - "[Establish native application foundation](../../story/doing/establish-native-application-foundation.md)"
depends_on:
  - "[Integrate and validate the Herdr terminal platform](../todo/integrate-and-validate-herdr-terminal-platform.md)"
blocks:
  - "[Validate mobile platform presentation](../todo/validate-mobile-platform-presentation.md)"
  - "[Validate the mobile terminal simulator product](validate-mobile-terminal-simulator-product.md)"
---

# Keep Herdr authoritative for terminal topology

Keep feature state and shared terminal UI independent from platform presentation while platform adapters consume the same Herdr Session to Workspace to Tab to Pane hierarchy. Focused app and UI tests must prove that no parallel topology model is introduced. Contract-alignment work may proceed, but the real commercial Herdr source and external Pane backend are unavailable and delivery remains blocked on their approved integration and validation.
