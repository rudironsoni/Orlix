---
type: architecture-decision
tags:
  - architecture
  - decision
  - native-app-foundation
updated: 2026-08-29
status: accepted
external_id: "ADR-0032"
summary: "Keep vvterm history external and synchronize exact source snapshots through a reviewed three-way merge."
part_of:
  - "[Orlix](../product/orlix.md)"
amends:
  - "[ADR 0024](0024-adopt-orlix-native-application-foundation.md)"
targets:
  - "[Establish native application foundation](../story/doing/establish-native-application-foundation.md)"
---

# ADR 0032: Sync vvterm With Three-Way Source Snapshots

## Status

Accepted.

## Context

Orlix must keep the complete native application editable in one Xcode project. It must also update from exact vvterm commits without importing vvterm history into Orlix history. Submodules split the Xcode workspace and checkout state. Subtrees mix repository histories. Blind snapshot replacement hides conflicts with Orlix branding and OrlixOS integration.

## Decision

Keep vvterm as tracked Orlix source files under `Orlix`. Record the exact upstream commit and tree in the release manifest. Do not import upstream Git objects or ancestry into the Orlix repository.

Synchronize with `make vvterm-sync VVTERM_COMMIT=<full-commit>`. The workflow fetches the old and new upstream snapshots into temporary repositories, applies the versioned branding policy to both, and performs a three-way merge against the tracked Orlix source. It copies only the merged files back into the workspace. Reviewed conflict decisions are explicit per-path manifests under `Orlix/make/vvterm-resolutions`.

Legal attribution stays unchanged. Product identity, bundle identifiers, repository links, themes, and source paths use a versioned branding policy. OrlixOS local-terminal integration and Orlix telemetry remain manual overlays because they change runtime ownership. `project.yml` remains the authoritative Xcode definition.

`make vvterm-sync-complete VVTERM_COMMIT=<full-commit>` must reject conflict markers and unbranded product inputs, then update immutable provenance. `make vvterm-source-check`, `make vvterm-sync-tests`, release-input checks, upstream tests, and Orlix app tests are required verification surfaces. Missing external Xcode components remain explicit blockers, not successful evidence.

## Consequences

The whole application remains directly editable in Xcode. Orlix history contains normal source commits only. Each update has an exact upstream base, an exact upstream target, a reviewed conflict map, and a reproducible branding policy. Upstream changes can still require manual source and package adaptation.

## Rejected Alternatives

- Git submodules, because they create separate checkout state and a split editing workflow.
- Git subtrees, because they mix upstream and product histories.
- A package-only wrapper, because Orlix needs to edit and build the complete application source.
- Blind file replacement or a patch queue without a three-way base, because both lose rename and conflict context.
