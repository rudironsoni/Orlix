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
- Added a pure semantic-frontier function and fixtures proving order independence, lowest-level selection, and exclusion of historical remediation gates.
- The live selector is intentionally unchanged in this checkpoint. The fixtures establish the behavior boundary required for the next change.
- Report-schema validation exposed and corrected a pre-existing contract bug: scoped `readiness_gate_eligible` evidence requires a passing real-stack report but does not itself authorize a global runtime-readiness claim. Release eligibility remains strict.
- Evidence: `swiftc -parse` passed for both Swift harness tools; `semantic-frontier-check`, `tcti-report-schema-check`, `tcti-plan-consistency`, `agent-harness-check`, and `git diff --check` passed.
