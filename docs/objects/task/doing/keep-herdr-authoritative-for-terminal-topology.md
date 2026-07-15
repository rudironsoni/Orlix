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
---

# Keep Herdr authoritative for terminal topology

Keep feature state and shared terminal UI independent from platform presentation while platform adapters consume the same Herdr topology. Focused app and UI tests must prove that no parallel topology model is introduced.
