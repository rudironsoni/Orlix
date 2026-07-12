# IMPLEMENT.md

## 2026-07-12

### Checkpoint: Plan Activation

- Created the project-wide Orlix agentic validation and release harness plan.
- The plan replaces historical first-unresolved gate selection with an L0-L5 pyramid frontier and requires symmetric simulator and physical product matrices before TestFlight.
- The first implementation milestone is harness inventory and gate-model extraction. Product runtime changes are not authorized by this checkpoint.
- Existing generated reports remain evidence inputs. They are not edited or treated as source.
- Required initial verification: plan consistency, full agent harness check, and `git diff --check`.

### Checkpoint: Inventory And Frontier Fixture

- Inventoried 115 current roadmap gates into the L0-L5 pyramid. Forty historical reducers, fixes, diagnostics, and root-cause entries belong outside the permanent product frontier.
- Confirmed the command ownership boundary: focused `tcti-gate` for L0-L2, `runtime-validation` for symmetric L3/L4 execution, and serialized existing beta phases for L5.
- Identified `selectedStatusWithSafety` as the narrow selector seam. Status generation, envelope generation, goal-loop execution, and envelope revalidation can remain stable while ranking is replaced.
- Added a pure semantic-frontier function and synthetic fixtures covering initial order independence, lowest-level selection, and historical-remediation exclusion cases.
- The live selector is intentionally unchanged in this checkpoint. The fixtures establish the behavior boundary required for the next change.
- Report-schema validation exposed and corrected a pre-existing contract bug: scoped `readiness_gate_eligible` evidence requires a passing real-stack report but does not itself authorize a global runtime-readiness claim. Release eligibility remains strict.
- Evidence: `swiftc -parse` passed for both Swift harness tools; `semantic-frontier-check`, `tcti-report-schema-check`, `tcti-plan-consistency`, `agent-harness-check`, and `git diff --check` passed.

### Checkpoint: Hook Promotion Guards

- Extended the shared Codex hook policy to consume a parseable TCTI task envelope whose traceability SHA matches `HEAD` before protected source mutation, phone execution, release execution, or permission escalation.
- TCTI runtime-source writes now require `runtime_patch_allowed=true`, current envelope authorization, and every protected target to match `allowed_scope`. Permission requests cannot override the envelope.
- Phone commands are blocked while `physical_device_allowed=false`. Archive, export, and upload commands are blocked while `release_gate_eligible=false`.
- Generated AgentHarness state, runtime reports, TCTI reports, reducers, generated kernel trees, generated mlibc trees, and generated OrlixOS trees are read-only to direct agent edits. Bare, `rtk`, and `rtk proxy` command prefixes share the tested mutation detection.
- Unambiguous external-SSD bypasses are blocked for executable Xcode commands: direct Apple tool paths and custom Xcode storage flags. Read-only searches and diagnostics may still mention external paths.
- Hook-maintenance fixtures may describe protected paths only when every patch target is under `.codex/hooks/`; mixed hook and product patches remain protected.
- Evidence: `rtk proxy /usr/bin/python3 -m unittest discover -s .codex/hooks/tests -p 'test_*.py'` passed 46 tests; `rtk proxy /usr/bin/python3 -m py_compile .codex/hooks/orlix_hook_common.py .codex/hooks/pre_tool_use_guard.py .codex/hooks/permission_request_guard.py` passed; `rtk proxy make agent-hooks-check` passed; `rtk proxy make agent-harness-check` passed; `rtk git diff --check` passed.
- Final checkpoint state: hook promotion guards are verified and ready to commit; product runtime, simulator, device, and release readiness remain unchanged.

### Review Findings Before Live Frontier Wiring

- The live selector remains unchanged. Existing permanent simulator gates still depend on historical reducers and fixes, so excluding remediation candidates without migrating those prerequisites can produce no valid permanent frontier.
- Pyramid level and remediation disposition must become explicit validated roadmap metadata. Silent fallback inference and substring classification are not strong enough for release promotion.
- Same-level acceptance ordering requires an explicit policy and fixtures. `blocker` work must not be accidentally delayed behind broader readiness claims.
- Scoped readiness eligibility must be tied to a canonical gate contract rather than accepted from an arbitrary report boolean.
- Required next model checkpoint: migrate explicit `pyramid_level` and disposition metadata, validate the real-roadmap inventory, remove historical remediation from permanent prerequisites, and add dependency-aware fixtures before replacing live selection.

### Checkpoint: Explicit Frontier Metadata

- Added compact top-level roadmap lists for explicit L0 through L5 membership and historical-remediation disposition. This avoids repeating metadata inside all 115 large gate objects while making classification independent of `kind` and `proof_tier` inference.
- Roadmap validation now requires every gate ID to appear exactly once in either one pyramid level or the historical-remediation list. Unknown IDs, missing IDs, duplicate membership, and overlap fail validation.
- Locked the reviewed migration inventory at L0=4, L1=25, L2=29, L3=16, L4=1, L5=0, and historical remediation=40 until the compact gate graph adds or removes capabilities intentionally.
- Removed three historical remediation prerequisites from permanent simulator gates: runtime stability, Linux console usability, and full shell usability. Historical reducers and fixes remain available for dynamic selection after a current failure.
- The pure semantic frontier now requires explicit level and disposition inputs. Silent L1 fallback and substring-based permanent-gate classification are removed from this decision boundary.
- Same-level ordering now evaluates acceptance weight before result state, selecting blockers before readiness, release, and probes. A cross-state fixture proves that a stale blocker precedes a failing readiness gate.
- The live selector remains unchanged pending a real-roadmap status fixture and explicit dynamic reducer scheduling.
- Evidence: `rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift` passed; `rtk proxy .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift validate-roadmap` passed; `rtk proxy .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift semantic-frontier-check` passed; `rtk git diff --check` passed.
- Adversarial review found and closed one comparator defect: acceptance weight now precedes state, and the cross-state blocker/readiness fixture passes. The reviewer reported no remaining blocking comparator finding.
- Final verification also passed `rtk proxy make agent-harness-check`, `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`, and `rtk proxy make tcti-gate TARGET=tcti-report-schema-check`.

### Checkpoint: Reducer-First Runtime Authorization

- Corrected the result classifier so a current simulator, component, Xcode, or product failure no longer authorizes an immediate runtime patch.
- Raw current failures emit `runtime_patch_allowed=false`, stop, and require a replayable reducer.
- Runtime authorization remains disabled even for a production-fix gate with a reducer-shaped prerequisite name. Names and prerequisite state are not sufficient proof of reducer replay, exact failure linkage, freshness, or ownership.
- Added classifier fixtures for raw kernel failure, Xcode/product failure, simulator install product rejection, and a misleading reducer-named prerequisite.
- A future authorization checkpoint must add structured reducer report identity, replay status, exact failing-report and failure-ID linkage, execution freshness, and owning-layer evidence before any `runtime_patch_allowed=true` path is introduced.
- This tightens authorization only. It does not select a reducer automatically and does not change product runtime behavior.
- Adversarial re-review found no remaining blocker and confirmed there is no current classifier path that emits `runtime_patch_allowed=true`.
- Evidence: `rtk proxy .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift gate-result-policy-check`, Swift parse, `rtk proxy make agent-harness-check`, and `rtk git diff --check` passed.

### Checkpoint: Active Goal And Luna Workflow

- Activated `docs/goals/active/orlix-release.md` as a concise map to the existing L0-L5 plan and completion contract.
- Added `WORKFLOW.md` with disjoint reducer-contract, evidence-inventory, and adversarial-authorization packets.
- Ran the three packets with `gpt-5.6-luna` at medium reasoning. They were read-only and edited no product or harness source.
- The agents agreed that current reducer descriptors already carry replay command, case, artifacts, and expected status, while current reports carry structured failure IDs and the next-step consumer already computes semantic execution freshness.
- Missing authorization-grade evidence is exact failing-report content identity, structured failure fingerprint, explicit replay outcome, exact causal linkage, canonical source owner, repository-derived owner scope, and an authorization input digest.
- The adversarial review rejected trusting `git_sha == HEAD`, artifact-path membership, filenames, prerequisite names, free-form evidence, or a producer-provided authorization boolean.
- The workflow now records a six-step fail-closed implementation sequence. `runtime_patch_allowed` remains false until an end-to-end positive authorization fixture and all forged, stale, unrelated, mismatched, mixed, and extra-target fixtures pass.
- Validation: the active goal is 2,075 characters; plan consistency passed; a serial `rtk proxy make agent-harness-check` passed; `rtk git diff --check` passed.
- One earlier full-harness run transiently failed its temporary kernel KUnit fixture. An isolated reproduction emitted every expected field and satisfied the exact predicate, and the serial rerun passed. No source change was made for that transient result.
- Final checkpoint validation passed plan consistency, hook checks, and `git diff --check`; the goal and workflow are ready to commit.

### Checkpoint: Typed Reducer Linkage Schema

- Added optional closed-world `source_failure` linkage to reducer descriptors and reports with an approved repository-relative report path, report SHA-256, structured failure ID, and failure fingerprint. Canonical filesystem normalization and symlink containment remain required before authorization is enabled.
- Added closed replay outcomes: `reproduced`, `not_reproduced`, `not_run`, `different_failure`, `environment_failure`, `harness_failure`, `forbidden_behavior`, and `inconclusive`.
- Existing reducers remain compatible because the new fields are optional. Real gate call sites do not emit linkage yet.
- Schema validation rejects malformed or unapproved report paths, invalid digests, empty failure identities, unknown replay outcomes, unpaired linkage/outcome fields, missing or unknown linkage keys, and an attempted producer-controlled `runtime_patch_allowed` key.
- The next-step consumer decodes the same typed fields into report facts and exposes a focused structured-linkage fixture check. It does not perform causal authorization yet.
- Added a narrow repository-controlled source-owner policy for `orlix-kernel-tcti`; its scopes cannot point at generated trees, absolute paths, or traversal paths. No other product owner is authorization-capable yet.
- `runtime_patch_allowed` remains false on every classifier path. This checkpoint defines data shape only and changes no product runtime behavior.
- Focused evidence: Swift parse and typecheck passed for the TCTI gate; Swift parse passed for next-step; `structured-reducer-linkage-check` passed; `tcti-report-schema-check` passed; `git diff --check` passed.
- Adversarial review initially found producer/consumer root drift, permissive owner-policy decoding, stale active-plan authorization truth, and missing pairing fixtures. All four findings were corrected and the same reviewer confirmed no remaining finding in that scope.
- Final evidence also passed `source-owner-policy-check`, `tcti-plan-consistency`, and the full serial `agent-harness-check`.

### Checkpoint: Package Failure Source Linkage

- The existing `tcti-package-behavior` runtime failure path now finalizes its JSON report before writing the report-specific reducer.
- The source report owns one stable structured failure ID and fingerprint derived from gate, destination, failure context, product version/build, and simulator runtime identity. Explanatory prose, timestamps, report paths, and process IDs are excluded.
- The reducer records the repository-relative source report path, SHA-256 of the exact finalized report bytes, matching failure ID and fingerprint, and `replay_outcome=not_run`.
- Reducer emission fails closed when the source report is missing, outside `Build/Reports/runtime`, non-failing, or lacks the matching structured failure.
- The focused fixture invokes the real `die()` ordering, verifies deterministic identity, exact digest linkage, changed-input behavior, pre-finalization rejection, and absence of `runtime_patch_allowed`.
- This is producer linkage only. It does not claim replay reproduction, choose an owning source layer, or authorize runtime edits. `runtime_patch_allowed` remains false.
- No OrlixKernel, OrlixMLibC, OrlixOS, HostAdapter, app runtime, generated upstream source, physical-device behavior, production assembly, or gadget dispatch changed.

### Checkpoint: XCTest Selected-Suite Finalization

- The selected mlibc dynamic-loader XCTest executed once with zero failures, emitted `ORLIX-MLIBC-TEST-END`, and proved the dynamic-loader marker and PT_INTERP workload before Xcode stalled in post-test finalization.
- The shared parser now accepts either Xcode's final success banner or a clean selected-suite summary with at least one executed test, zero failures, and the required completion marker. Explicit failure evidence still rejects the run.
- All six mlibc gates using this runner stop Xcode after the complete evidence set instead of waiting indefinitely for post-test finalization.
- Focused parser fixtures cover a clean selected-suite pass, nonzero failures, and a missing completion marker.
- The corrected dynamic-loader rerun passed with one test executed, one passed, zero failed, zero skipped, and all forbidden behavior false.
- The subsequent selected pthread/TLS rerun also passed after the remaining mlibc call sites adopted the shared parser.
- This changes gate process control and evidence parsing only. Product runtime behavior is unchanged.

### Checkpoint: OCI XCTest Selected-Suite Finalization

- The selected OCI stdio/signal/wait XCTest executed once with zero failures before Xcode stalled after the selected-suite summary.
- The five OCI component gates using the same file-backed runner now use the shared clean selected-suite parser and stop after complete XCTest evidence instead of waiting for Xcode's final banner.
- Every OCI gate retains its existing named-test, skip, runtime assertion, artifact, and exit-status requirements; a clean XCTest summary alone cannot satisfy those gate-specific contracts.
- The corrected stdio/signal/wait rerun passed. This changes harness process control only and does not change OCI, OrlixOS, Linux, HostAdapter, or app behavior.

### Checkpoint: Missing Pinned-Simulator Artifact Continuation

- The documented safe-generator policy recognized only exact `simulator-runtime` kinds, so the missing `simulator-runtime-real-stack` mlibc report stopped even though its exact pinned-simulator command and prerequisite were valid.
- Safe generator recognition now accepts only the explicit `simulator-runtime` and `simulator-runtime-real-stack` kinds and one seven-token command grammar with a constrained gate name and exact pinned destination, simulator ID, and simulator name arguments.
- Classifier fixtures prove the missing pinned real-stack mlibc report continues only through its exact runtime-validation generator and reject malicious kinds, altered simulator tokens, duplicate destinations, appended shell commands, physical-device status, and unsatisfied prerequisites.
- Physical-device commands, unpinned simulator commands, and unknown generators remain stop conditions.

### Checkpoint: Frontier Activation Prerequisite Correction

- Regenerated current harness state after the supplementary simulator reports. All permanent simulator readiness gates pass, but the old live selector incorrectly chooses historical `tcti-direct-chain-fuzz` work.
- Audited the explicit permanent frontier before replacing the selector. The roadmap currently contains sixteen L3 gates, only one L4 first-syscall gate, and no L5 gates.
- The existing physical first-syscall report passes, so directly switching to the pure semantic frontier would leave no selected permanent work. It would not represent the required identical phone matrix or TestFlight completion.
- Corrected the implementation order: define one canonical product-capability matrix with simulator and approved-device instances, derive exact matrix and product-identity equality, add L5 release gates, then activate safety-filtered semantic frontier selection.
- The model must reuse semantic acceptance across destinations. It must not duplicate test meaning in destination-specific Make targets. Destination adapters remain limited to launch and evidence capture.
- Physical selection remains blocked unless the complete current simulator matrix, explicit opt-in, clean protected worktree, and destination policy all pass. Release remains blocked until the complete identical phone matrix passes for the same product identity.
- No product runtime code, generated report, reducer, upstream clone, phone execution, archive, or release action changed in this checkpoint.

### Checkpoint: Canonical Product Capability Inventory

- Added one closed-world `product_runtime_capabilities` map covering every current L3 permanent gate. Each capability names its existing simulator gate aliases, optional materialized device gate, runtime-validation gate, marker artifact, and marker when required.
- The two existing first-syscall L3 aliases now map to one semantic capability and the one existing L4 first-syscall gate. The model does not invent future device gate IDs.
- Roadmap validation now rejects missing or extra L3 capability coverage, duplicate capability/runtime/simulator/materialized-device identities, unknown or non-simulator gates, inexact runtime commands, invalid materialized L4 gates, and empty or incomplete marker contracts.
- Focused fixtures validate the real roadmap and independently reject missing coverage, duplicate identities or runtime semantics, runtime-gate prefix collisions, unpinned simulator commands, and device gates outside L4.
- This checkpoint defines semantic identity once. It does not add destination-specific acceptance logic, authorize phone execution, claim L4 equality, add L5 gates, or activate the live semantic frontier.
- Current report inspection also confirmed that runtime reports expose version and build identity but do not yet expose the semantic product/payload identity required for final L3/L4 equality. That remains required before release promotion.
- No product runtime code, generated report, reducer, upstream clone, phone execution, archive, or release action changed.
