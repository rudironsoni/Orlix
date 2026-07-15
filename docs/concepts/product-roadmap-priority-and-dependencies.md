---
type: concept
tags:
  - roadmap
  - guidance
updated: 2026-07-15
summary: "Order Orlix epics, stories, and tasks by their hard delivery dependencies, parallel work, optional proof, and external approvals."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Product roadmap priority and dependencies

This page records the durable execution order across the complete epic, story, and task graph. Object status belongs to the individual ontology pages, and the current executable TCTI gate belongs to the [structured task envelope](../../Build/AgentHarness/orlix-tcti/next-task.md). Do not copy volatile gate status into this roadmap.

## Dependency semantics

- A `depends_on` edge is a hard delivery prerequisite. Its inverse `blocks` edge must exist on the prerequisite page.
- Work without a hard edge may proceed in parallel when ownership and available capacity permit it.
- Commercial, maintainer, device, upload, and store actions are external authorization gates. A documentation task records the gate but does not grant authorization.
- Physical-device validation is optional until selected for a candidate. Once selected, it must use the simulator-proven fingerprint and becomes a promotion requirement for that candidate.
- Priority describes the intended critical-path order, not current execution status. Completed work remains in the matrix so the dependency model covers every object.

## Priority sequence

1. **P0, preserve the harness and start long-lead approvals.** Keep the ontology and structured reports authoritative, and begin Herdr, mobile terminal, and downloaded-content approval work because external lead time can run in parallel.
2. **P1, complete the pinned TCTI simulator gate.** The [current TCTI task envelope](../../Build/AgentHarness/orlix-tcti/next-task.md) selects the exact command. Local session binding, Herdr topology work, and OCI image import may progress in parallel without claiming the blocked product tiers.
3. **P2, unlock the product foundations.** After the pinned simulator proof, integrate the approved Herdr platform, implement and prove Local Instance isolation, complete authorized TCTI device proof when authorized, promote TCTI, and validate mobile presentation.
4. **P3, validate and publish the mobile terminal release.** The first public release proves terminal, Herdr, Local Runtime, TCTI, and mobile presentation. It does not claim OCI or Docker behavior.
5. **P4, complete OCI lifecycle and Docker compatibility.** Build on proven Local Instance isolation, imported OCI content, and the released terminal foundation.
6. **P5, validate and publish the mobile container release.** Add OCI, Docker, and downloaded-content behavior while preserving the mobile terminal contract.
7. **P6, implement and publish native macOS.** Reuse the two proven mobile product contracts, then add the native Mac target and host integration.
8. **P7, expose the stable TCTI harness through MCP.** Stabilize report contracts before adding the read interface so workflow tooling cannot freeze a volatile schema.

## Epic and story dependency matrix

| Epic | Story | Priority | Hard predecessors | Unlocks |
| --- | --- | --- | --- | --- |
| [Orlix agent harness](../objects/epic/done/orlix-agent-harness.md) | [Establish ontology-backed agent harness](../objects/story/done/establish-ontology-backed-agent-harness.md) | P0, done | None | Every maintained roadmap and structured execution workflow |
| [Orlix TCTI](../objects/epic/doing/orlix-tcti.md) | [Prove TCTI product execution](../objects/story/doing/prove-tcti-product-execution.md) | P1-P2 | Current structured task envelope | Native Herdr integration, Local Instances, OCI lifecycle, and the mobile terminal release |
| [Native app foundation](../objects/epic/doing/native-app-foundation.md) | [Establish native application foundation](../objects/story/doing/establish-native-application-foundation.md) | P0-P2 | Herdr approval and pinned TCTI simulator proof | Mobile terminal release |
| [Native app foundation](../objects/epic/doing/native-app-foundation.md) | [Deliver Local Runtime and Local Instances](../objects/story/doing/deliver-local-runtime-and-instances.md) | P1-P2 | Local session binding and pinned TCTI simulator proof | OCI lifecycle and mobile terminal release |
| [Orlix release](../objects/epic/doing/orlix-release.md) | [Validate and publish the mobile terminal release](../objects/story/doing/validate-and-publish-mobile-terminal-release.md) | P3 | Native application, Local Runtime, and TCTI product stories | Mobile container release |
| [OCI-derived environments](../objects/epic/doing/oci-derived-environments.md) | [Deliver OCI environment lifecycle](../objects/story/doing/deliver-oci-environment-lifecycle.md) | P1, P4 | Local Runtime and Local Instances | Mobile container release |
| [Orlix release](../objects/epic/doing/orlix-release.md) | [Validate and publish the mobile container release](../objects/story/todo/validate-and-publish-mobile-container-release.md) | P5 | Mobile terminal release and OCI lifecycle | Native macOS release |
| [Orlix release](../objects/epic/doing/orlix-release.md) | [Validate and publish the native macOS release](../objects/story/todo/validate-and-publish-native-macos-release.md) | P6 | Mobile container release | Native Mac publication |
| [TCTI MCP](../objects/epic/todo/tcti-mcp.md) | [Expose the TCTI harness through MCP](../objects/story/todo/expose-tcti-harness-through-mcp.md) | P7 | Stable TCTI report contract and completed TCTI workflow | Read-oriented workflow integration |

The release epic spans three stages, so its completion depends on native, TCTI, and OCI work even though the mobile terminal story can finish before OCI and Docker. Story edges define the promotion order within that epic.

## Complete task dependency matrix

| Priority | Task | Hard predecessors | Unlocks |
| --- | --- | --- | --- |
| P0, done | [Migrate durable knowledge into the ontology](../objects/task/done/migrate-durable-knowledge-into-ontology.md) | None | Typed roadmap maintenance |
| P0, done | [Route execution state to structured reports](../objects/task/done/route-execution-state-to-structured-reports.md) | None | Current task envelopes and evidence |
| P1, done | [Make app-hosted XCTest sessions terminate cleanly](../objects/task/done/make-app-hosted-xctest-sessions-terminate-cleanly.md) | Local session runtime | Reliable app-hosted lifecycle verification |
| P0, external | [Secure Herdr commercial integration](../objects/task/todo/secure-herdr-commercial-integration.md) | None | Herdr integration |
| P0, external | [Secure mobile terminal distribution approval](../objects/task/todo/secure-mobile-terminal-distribution-approval.md) | None | Mobile terminal archive and upload |
| P0, external | [Secure downloaded-content distribution approval](../objects/task/todo/secure-downloaded-content-distribution-approval.md) | None | Mobile container simulator and archive gates |
| P1 | [Complete the pinned simulator TCTI ladder](../objects/task/doing/complete-pinned-simulator-tcti-ladder.md) | Current TCTI task envelope | Herdr integration, Local Instance lifecycle, authorized TCTI device proof |
| P1 | [Bind local sessions through OrlixOS](../objects/task/doing/bind-local-sessions-through-orlixos.md) | Existing terminal transport and console policy | Local Instance lifecycle and mobile presentation proof |
| P1 | [Keep Herdr authoritative for terminal topology](../objects/task/doing/keep-herdr-authoritative-for-terminal-topology.md) | Validated Herdr integration | Mobile presentation and terminal simulator proof |
| P1, parallel | [Import OCI image content](../objects/task/doing/import-oci-image-content.md) | None | OCI runtime lifecycle |
| P2 | [Integrate and validate the Herdr terminal platform](../objects/task/todo/integrate-and-validate-herdr-terminal-platform.md) | Herdr approval and pinned TCTI simulator proof | Authoritative Herdr topology |
| P2 | [Implement namespaced Local Instance lifecycle](../objects/task/todo/implement-namespaced-local-instance-lifecycle.md) | Local session binding and pinned TCTI simulator proof | Concurrent Local Instance isolation |
| P2 | [Prove concurrent Local Instance isolation](../objects/task/todo/prove-concurrent-local-instance-isolation.md) | Namespaced Local Instance lifecycle | OCI runtime lifecycle and terminal simulator proof |
| P2, external | [Run authorized TCTI device validation](../objects/task/todo/run-authorized-tcti-device-validation.md) | Pinned TCTI simulator ladder | TCTI product-default promotion |
| P2 | [Promote TCTI as the product default](../objects/task/todo/promote-tcti-as-product-default.md) | Authorized TCTI device validation | Mobile terminal simulator proof |
| P2 | [Validate mobile platform presentation](../objects/task/todo/validate-mobile-platform-presentation.md) | Herdr topology and local session binding | Mobile terminal simulator proof |
| P3 | [Validate the mobile terminal simulator product](../objects/task/doing/validate-mobile-terminal-simulator-product.md) | Herdr topology, mobile presentation, Local Instance isolation, TCTI product default | Optional device proof and terminal archive |
| P3, optional external | [Validate an authorized mobile terminal device](../objects/task/todo/validate-authorized-mobile-terminal-device.md) | Mobile terminal simulator proof | Candidate-specific promotion when selected |
| P3, external upload | [Archive, export, and upload the mobile terminal release](../objects/task/todo/archive-export-and-upload-mobile-terminal-release.md) | Mobile terminal simulator proof and distribution approval | Mobile container simulator proof |
| P4 | [Implement OCI runtime lifecycle](../objects/task/todo/implement-oci-runtime-lifecycle.md) | OCI image import and Local Instance isolation | Docker compatibility |
| P4 | [Prove Docker engine compatibility](../objects/task/todo/prove-docker-engine-compatibility.md) | OCI runtime lifecycle | Mobile container simulator proof |
| P5 | [Validate the mobile container simulator product](../objects/task/todo/validate-mobile-container-simulator-product.md) | Docker compatibility, downloaded-content approval, mobile terminal archive | Optional device proof and container archive |
| P5, optional external | [Validate an authorized mobile container device](../objects/task/todo/validate-authorized-mobile-container-device.md) | Mobile container simulator proof | Candidate-specific promotion when selected |
| P5, external upload | [Archive, export, and upload the mobile container release](../objects/task/todo/archive-export-and-upload-mobile-container-release.md) | Mobile container simulator proof and downloaded-content approval | Native macOS implementation |
| P6 | [Implement the native macOS product](../objects/task/todo/implement-native-macos-product.md) | Mobile container archive | Native macOS validation |
| P6 | [Validate the native macOS product](../objects/task/todo/validate-native-macos-product.md) | Native macOS implementation | Native macOS archive |
| P6, external upload | [Archive, export, and upload the native macOS release](../objects/task/todo/archive-export-and-upload-native-macos-release.md) | Native macOS validation | Native Mac publication |
| P7 | [Stabilize the TCTI report contract](../objects/task/todo/stabilize-tcti-report-contract.md) | Mature structured TCTI reports | TCTI MCP read interface |
| P7 | [Define the TCTI MCP read interface](../objects/task/todo/define-tcti-mcp-read-interface.md) | Stable TCTI report contract | Read-oriented TCTI workflow access |

## Governing decisions and evidence

The proof order follows [ADR 0017](../objects/architecture-decision/0017-product-runtime-claim-promotion-order.md) and the app-hosted requirement follows [ADR 0018](../objects/architecture-decision/0018-ground-kernel-proof-in-app-hosted-runtime.md). Execution, distribution, application, terminal, Local Instance, host-integration, and Docker boundaries follow [ADR 0022](../objects/architecture-decision/0022-use-hosted-linux-elf-execution.md), [ADR 0023](../objects/architecture-decision/0023-use-release-development-profiles-and-curated-orlixos-distribution.md), [ADR 0024](../objects/architecture-decision/0024-adopt-orlix-native-application-foundation.md), [ADR 0025](../objects/architecture-decision/0025-make-herdr-authoritative-for-terminal-topology.md), [ADR 0026](../objects/architecture-decision/0026-use-one-kernel-with-namespaced-local-instances.md), [ADR 0027](../objects/architecture-decision/0027-use-app-store-only-cross-platform-host-integration.md), and [ADR 0028](../objects/architecture-decision/0028-provide-full-docker-engine-compatibility-through-orlixos.md).

Release gates consume the checked-in [release input manifest](../sources/release/orlix-app-release-inputs.json), [source provenance](../sources/release/app-source-provenance.md), and [privacy and license audit](../sources/release/privacy-and-license-audit.md). These inputs and the structured reports provide evidence. They do not replace the runtime proof tier required by the owning task.
