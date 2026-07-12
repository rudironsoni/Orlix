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
