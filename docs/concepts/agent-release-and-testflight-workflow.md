---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-08-29
summary: "Repository agents use the ontology for durable context and structured Build reports for current state."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Agent, release, and TestFlight workflow

Repository agents use the ontology for durable context and structured Build reports for current state. Release gates consume checked-in source inputs, then archive, export, validate, and upload using explicit operator-controlled steps.

The release workflow uses target-derived marketing version, build number, bundle identifier, signing team, entitlements, and payload metadata. Before archive, validate the checked-in release manifest, source provenance, privacy declarations, license audit, build configuration, and complete simulator product matrix. Validate the archive before export, then validate the exported application and IPA before upload.

Uploading and assigning a TestFlight build are external state changes. They require explicit operator authorization and must record the exact archive, IPA, product identity, upload result, App Store Connect processing state, Beta App Review state, group assignment, and public-link state in the release report. A successful archive or upload cannot establish product runtime readiness.

Every reusable command and classifier belongs to the release harness. Durable policy belongs in this concept and the owning decisions. Current results and blockers belong to `Build/AgentHarness/orlix-release/`.

An `ios-v<marketing-version>` tag on `origin/main` starts the beta workflow. The workflow uses pinned GitHub Actions, Xcode 26.6, and pinned Fastlane. It creates one runtime build number without changing `project.yml`, runs the simulator and release gates, signs and exports the application, uploads the exact IPA, waits for App Store Connect processing, and assigns the build to one existing internal TestFlight group. The release report binds the repository, tag, commit, workflow run, version, build number, IPA hash, Xcode version, upload backend, processing state, and group.

Production promotion is a separate manually dispatched workflow protected by the `app-store-review` environment. It downloads the report from the successful beta workflow for the selected tag, validates the same commit and build identity, validates the checked-in metadata and screenshots, then submits that existing TestFlight build for App Review with automatic release disabled. The workflow must fail before either external action when public distribution approval is incomplete.
