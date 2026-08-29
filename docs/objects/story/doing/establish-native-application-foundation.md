---
type: story
tags:
  - story
  - native-app-foundation
updated: 2026-08-29
status: doing
summary: "Provide native mobile presentation over the commercially approved Herdr terminal platform once its external gates pass."
story_of:
  - "[Native app foundation](../../epic/doing/native-app-foundation.md)"
has_task:
  - "[Synchronize vvterm upstream source](../../task/doing/synchronize-vvterm-upstream-source.md)"
  - "[Secure Herdr commercial integration](../../task/todo/secure-herdr-commercial-integration.md)"
  - "[Integrate and validate the Herdr terminal platform](../../task/todo/integrate-and-validate-herdr-terminal-platform.md)"
  - "[Keep Herdr authoritative for terminal topology](../../task/doing/keep-herdr-authoritative-for-terminal-topology.md)"
  - "[Validate mobile platform presentation](../../task/todo/validate-mobile-platform-presentation.md)"
blocks:
  - "[Validate and publish the mobile terminal release](validate-and-publish-mobile-terminal-release.md)"
---

# Establish native application foundation

As an Orlix user, I want native iOS and iPadOS presentation over one authoritative terminal topology so remote and local sessions behave consistently without duplicating OS or terminal ownership.

The story is complete when the currently unavailable commercial Herdr source and external Pane extension are approved and integrated, the real Herdr platform owns Session, Workspace, Tab, Pane, and focus topology, and the iOS and iPadOS adapters pass focused presentation tests. Native macOS implementation and validation belong to the later macOS release story.
