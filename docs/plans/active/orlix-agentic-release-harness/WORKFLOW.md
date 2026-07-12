# Workflow

## R1: Reducer Contract

- Objective: define the smallest structured evidence contract that can safely authorize a runtime patch.
- Scope: next-step models, report schema, existing reducer reports, active plan.
- Output: required fields, validation rules, negative fixtures, and integration point.
- Do not: edit product runtime code or generated evidence.

## R2: Existing Evidence Inventory

- Objective: identify durable reducer, replay, failure-linkage, freshness, and owning-layer fields already emitted by current tools.
- Scope: TCTI gate source, next-step source, report fixtures, reducer readers, roadmap.
- Output: reusable fields, missing fields, and exact producer/consumer locations.
- Do not: propose filename or prose matching as authorization.

## R3: Authorization Review

- Objective: adversarially test the proposed reducer authorization boundary.
- Scope: classifier, task envelope, hooks, report schema, active plan.
- Output: bypass cases, fail-closed requirements, and acceptance fixtures.
- Do not: weaken simulator-before-phone or phone-before-release promotion.

## Integration

The manager integrates one minimal schema and classifier checkpoint, validates it with focused fixtures and the full harness, requests final review, updates `IMPLEMENT.md`, and commits only after all blocking findings are resolved.

## Integrated Decision

Runtime authorization will use a dedicated closed-world authorization object. The envelope boolean remains a derived summary and is never sufficient by itself.

Required causal identity:

```text
selected gate instance and semantic product identity
failing report normalized path and content digest
structured failure ID and failure fingerprint
reducer report normalized path and content digest
reducer case and canonical replay command
replay outcome=reproduced
matching reproduced failure ID and fingerprint
execution freshness for failure and replay
one canonical source owner
repository-derived owner scope
authorization input digest
```

The implementation order is:

1. Add typed reducer linkage and replay-outcome models plus schema fixtures. Keep authorization false.
2. Emit linkage from one current runtime-failure reducer producer and validate exact report/failure causality.
3. Add canonical source-owner vocabulary and checked-in owner-to-scope policy.
4. Recompute authorization in the envelope validator and hook verifier. Never trust a producer-provided boolean or arbitrary scope.
5. Add adversarial fixtures for forged envelopes, stale or superseded evidence, changed content at the same path, mismatched failure/build/destination/owner, mixed environment evidence, forbidden behavior, path escape, and extra mutation targets.
6. Enable `runtime_patch_allowed=true` only after one end-to-end positive fixture and every near-miss fixture pass.

Phone and release authorization remain independent. Reducer authorization cannot imply simulator readiness, physical-device permission, or release eligibility.
