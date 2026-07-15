---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-15
summary: "Repository agents use the ontology for durable context and structured Build reports for current state."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Agent, release, and TestFlight workflow

Repository agents use the ontology for durable context and structured Build reports for current state. Release gates consume checked-in source inputs, then archive, export, validate, and upload using explicit operator-controlled steps.

The release workflow uses target-derived marketing version, build number, bundle identifier, signing team, entitlements, and payload metadata. Before archive, validate the checked-in release manifest, source provenance, privacy declarations, license audit, build configuration, and complete simulator product matrix. Validate the archive before export, then validate the exported application and IPA before upload.

Uploading and assigning a TestFlight build are external state changes. They require explicit operator authorization and must record the exact archive, IPA, product identity, upload result, App Store Connect processing state, Beta App Review state, group assignment, and public-link state in the release report. A successful archive or upload cannot establish product runtime readiness.

Every reusable command and classifier belongs to the release harness. Durable policy belongs in this concept and the owning decisions. Current results and blockers belong to `Build/AgentHarness/orlix-release/`.
