# Orlix Agentic Validation, Release, And Harness Engineering

## Goal

Build a repository-native control system that lets agents move Orlix reliably through:

```text
L0 source and policy
-> L1 unit, oracle, and reducer
-> L2 component integration
-> L3 complete simulator product validation
-> L4 identical physical-device product validation
-> L5 TestFlight publication
```

Humans steer product intent and approve physical and release access. Agents inspect, plan, execute, review, repair, validate, publish, and maintain the harness.

Completion requires one Orlix app build to pass L3 and L4, emit app-visible `ORLIX-USERLAND-TCTI-OK` from real Linux userspace through OrlixKernel and TCTI on both destinations, keep all forbidden behavior false, and be confirmed in TestFlight.

## Harness Engineering Principles

- Repository knowledge is authoritative. Architecture, plans, gates, ownership, reports, and remediation guidance are versioned locally.
- Give agents a map. `AGENTS.md` is a concise index, with progressive disclosure through focused docs, skills, status, and task envelopes.
- Optimize for agent legibility. Every selected action has a stable command, structured inputs, machine-readable output, explicit owner, and reproducible evidence.
- Enforce invariants mechanically. Hooks and structural tests enforce ownership, dependency direction, generated-tree protection, promotion order, and forbidden behavior.
- Close the loop. Agents inspect, execute, observe, classify, repair, validate, review, commit, and advance without humans relaying logs.
- Capture judgment durably. Repeated corrections become tests, linters, stable failure IDs, skills, or architecture rules.
- Control entropy continuously. Recurring agents find duplicate gates, stale docs, giant selectors, dead paths, and unenforced rules.
- Treat human attention as scarce. Automate reproducible work and escalate only access decisions, product judgment, or true external blockers.

## Repository Knowledge

- `AGENTS.md` remains the project map and core invariant index.
- `docs/architecture`, `docs/adr`, and `docs/reference` remain architecture truth.
- `docs/harness` owns the pyramid, gate lifecycle, report contract, classifier policy, and operating guide.
- `docs/plans/active` owns current execution state, decisions, failures, and checkpoints.
- `.agents/skills` owns focused, agent-neutral workflows loaded only when selected.
- `.codex` remains a Codex adapter for hooks, permissions, and roles, not project identity.
- Mechanical checks validate indexes, links, owners, contradictory claims, generated-doc freshness, and oversized instruction surfaces.

## Canonical Gate Model

Permanent gates define only identity, pyramid level, owner, focused command, semantic inputs, prerequisites, destination applicability, report contract, and promotion weight.

Gate IDs describe capabilities rather than destinations. For example, `runtime.userland-marker` has `@iphonesimulator` and `@iphoneos` execution instances.

Historical bugs, reducers, proof refreshes, and one-time migrations are not permanent release gates. A current failure may dynamically select a reducer, harness repair, environment action, or owning-layer implementation task.

## L0: Source, Architecture, And Policy

Required coverage:

```text
project.yml version and build consistency
durable-input and generated-tree separation
upstream clone immutability
architecture dependency direction
Linux, HostAdapter, OrlixOS, and app ownership
TCTI executable-memory safety
x18, JIT, MAP_JIT, RWX, and forbidden API audit
report schema and structured failure IDs
toolchain and external-SSD environment health
knowledge-base structure and link integrity
```

L0 is fast, runs only when relevant inputs change, and emits remediation instructions naming the correct durable source. Forbidden behavior stops promotion.

## L1: Unit, Oracle, Golden, And Reducer

Required coverage:

```text
TCTI decode and execution semantics
register, flags, memory, and control flow
syscall, trap, and fault exits
switch-debug golden ELF oracle
memory-window and block-cache behavior
runtime log and marker extraction
report parsing and freshness logic
failure classifier and action authorization
component unit tests
replayable reducers derived from current higher-level failures
```

Golden and reducer reports prove narrow behavior only. They cannot claim product readiness or permanently preempt a stronger current product frontier.

## L2: Component Integration

Required coverage:

```text
OrlixKernel build, config, kselftest, syscall, execve, signals, wait, and PTY
OrlixMLibC build, Linux UAPI and sysdeps, libc subset, loader, pthread, and TLS
OrlixOS payload, rootfs, package manifest, session lifecycle, and OCI materialization
OrlixHostAdapter memory transport, console bridge, mediation, and ABI boundaries
app session lifecycle, terminal transport, and OrlixOS integration
Coreutils, package, and OCI lifecycle integration
```

Generated Linux, mlibc, Coreutils, package, OCI, rootfs, and build trees are read-only. Failures route to durable owning inputs.

## L3 And L4: Symmetric Product Runtime Matrix

Run the same matrix first on `iphonesimulator`, then unchanged on `iphoneos`:

```text
app launch
OrlixOS session and packaged payload
kernel boot
TCTI first syscall
runtime stability
Linux console output
static BusyBox start and command
full shell usability
mlibc-linked process
Coreutils command set
package behavior
dynamic loader
signals
VFS operations
OCI rootfs command
interactive terminal
full Linux runtime readiness
ORLIX-USERLAND-TCTI-OK
clean process and session exit
```

Every capability uses the same gate ID, Linux payload, argv, expected stdout and stderr, exit status, TCTI events, fatal policy, and forbidden-behavior policy.

Destination adapters own only discovery, install, launch, argument transport, log capture, crash collection, cleanup, and runtime identity. `simctl` and `devicectl` cannot define different semantic acceptance rules.

L3 uses pinned simulator `ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3`, `Orlix-iPhone-15-Pro-Max`, iOS 26.5. L4 uses approved device `00008130-001E74A11193803A`, `RRJ-iPhone-15-Pro-Max`, with its current recorded iOS build.

## Product Runtime Evidence

Every L3 and L4 report records gate, destination, product identity, semantic product fingerprint, traceability SHA, runtime identity, app identity, OrlixOS payload and session identity, Linux task and syscall events, terminal artifacts, marker provenance, crash reports, wall time, and forbidden behavior.

Marker validation rejects kernel command-line text, `execve argv` diagnostics, source references, report metadata, app-generated output, and harness-generated output. `ORLIX-USERLAND-TCTI-OK` passes only when emitted by real Linux userspace and captured through the app session path.

## L5: Archive And TestFlight

Required gates:

```text
release component build
Xcode archive
archive framework and payload validation
signing and entitlement validation
IPA export
TestFlight upload
App Store Connect build confirmation
```

L5 requires complete L3 and L4 matrices for the same marketing version, build number, and semantic product fingerprint. Release evidence records archive, IPA, bundle ID, signing team, product identity, upload timestamp, App Store Connect build, and processing state.

## Semantic Freshness And Reuse

Reports use semantic input fingerprints for freshness and retain `git_sha` only for traceability.

Semantic groups cover durable OrlixKernel, OrlixMLibC, OrlixOS, HostAdapter, app, package, OCI, TCTI, runtime-validation, toolchain, and `project.yml` inputs.

Generated reports, proof JSON, documentation, `IMPLEMENT.md`, agent configuration, roadmap metadata, and SHA-only changes do not invalidate product evidence.

A component change invalidates its checks and downstream consumers. A simulator runtime change invalidates only the matching L3 instance. A device OS change invalidates only the matching L4 instance. External SSD build products, TCTI binaries, payloads, installed apps, and archives are reused when semantic identities match.

## Status, Envelope, And Frontier

`agent-status` emits pyramid state, destination matrix, semantic freshness, blockers, and current frontier.

`agent-next` selects one eligible frontier gate. It never selects the first unresolved item from a historical list.

`next-task.json` contains the selected gate and instance, exact command, success criteria, invalidation reason, prerequisites, scope, skills, reports, classifier policy, verification, commit requirements, and stop conditions.

`agent-task-envelope-check` recomputes the envelope and rejects stale policy, mismatched metadata, wrong destination order, forbidden scope, and command drift.

Frontier selection:

1. Detect semantic input changes.
2. Invalidate affected gates and downstream dependents.
3. Find the lowest unresolved affected pyramid level.
4. Select one eligible high-value gate in that level.
5. Complete affected L0 through L2 prerequisites.
6. Complete the full L3 simulator matrix.
7. Require explicit physical authorization.
8. Complete the identical L4 device matrix.
9. Complete L5 and confirm TestFlight.
10. Mark the goal complete.

Unaffected maintenance state cannot preempt the product frontier. Reducers are selected only for current failures.

## Result Classification And Authorization

Result classes are pass, semantic refresh, missing artifact, environment failure, harness contract failure, component failure, simulator product failure, physical product failure, forbidden behavior, release failure, and TestFlight failure.

The classifier emits owning layer, runtime and harness patch authorization, retry and continuation policy, stop state, required action, and reason.

Runtime changes require current L2 through L4 evidence. Harness changes require a proven contract defect. Environment failures authorize no source changes. Forbidden behavior stops immediately.

## Autonomous Agent Loop

`make agent-goal AREA=orlix-release` performs:

```text
inspect -> select -> validate envelope -> execute -> observe -> classify
-> plan repair -> implement -> focused test -> product regression
-> agent review -> address feedback -> commit -> push -> advance
```

Planner, owner, reducer, safety reviewer, skeptical reviewer, and release reviewer are invoked only when required. L0 through L2 may use independent worktrees. Simulator, phone, signing, archive, and upload remain serialized.

Agents report immediately when a PR is pushed, feedback is addressed, a gate fails, or external access is required. Heartbeats monitor long work without replacing event-driven updates.

## Mechanical Guardrails

Hooks and structural tests block generated and upstream edits, wrong-layer Linux behavior, fake output, phone work before simulator completion and authorization, release before phone completion, unauthorized runtime patches, unauthorized assembly or gadget dispatch, external-SSD bypass, and unstructured or inconsistent reports.

Guardrail failures identify the invariant, owning layer, durable path, and exact recovery command.

## PR And Review Policy

Each PR advances one current frontier failure or one proven harness defect. It records the attempted product path, evidence, owner, focused tests, higher-level regression, and remaining truth.

Agents self-review, request specialized reviews, address actionable feedback, rerun affected checks, push updates, and keep PRs short-lived. Human review remains for product judgment, security and release decisions, or unresolved architecture tradeoffs.

## Entropy And Harness Gardening

Scheduled agents detect duplicate gates, oversized selectors, stale docs, dead report paths, missing owners, unstructured failures, obsolete aliases, unenforced invariants, and contradictory architecture claims.

Quality grades cover agent legibility, observability, isolation, architecture enforcement, failure classification, semantic freshness, and release reproducibility. Cleanup cannot preempt an active product frontier unless it fixes correctness or safety.

## Implementation Sequence

1. Preserve and classify interrupted command artifacts, then terminate orphaned processes.
2. Inventory current checks into L0 through L5 and identify duplicate, historical, superseded, and missing gates.
3. Define the compact gate graph, destination-independent IDs, semantic input groups, and report contracts.
4. Define one canonical L3/L4 product-capability list and instantiate it for `iphonesimulator` and `iphoneos`; destination instances share semantic inputs, acceptance, and product identity while adapters own only launch and evidence capture.
5. Derive L3 and L4 completion from the canonical matrix, and derive L5 eligibility from exact L3/L4 capability and product-identity equality. A single phone smoke gate or producer-owned readiness boolean is insufficient.
6. Replace historical first-unresolved selection with safety-filtered pyramid-frontier selection only after the permanent frontier can represent the complete L3, L4, and L5 objective.
7. Implement symmetric simulator and device destination adapters.
8. Shrink the roadmap and selector through progressive disclosure and focused skills.
9. Add mechanical architecture, generated-tree, metadata, and promotion checks.
10. Import current valid evidence without rerunning semantically unchanged work.
11. Add missing simulator userland-marker coverage and complete L3.
12. Run the identical matrix on the approved iPhone and complete L4.
13. Reduce and repair only concrete current failures with scoped, tested PRs.
14. Build, archive, validate, export, upload, and confirm the beta in TestFlight.
15. Enable recurring knowledge, quality, and harness gardening.
16. Remove obsolete compatibility aliases and historical permanent gates after migration validation.

## Acceptance

Harness fixtures cover semantic invalidation, proof-only changes, frontier selection, destination symmetry, reducer scheduling, authorization, generated-tree protection, L3-to-L4 promotion, L4-to-L5 promotion, resume behavior, and TestFlight confirmation.

Runtime fixtures cover identical payload construction, marker provenance, exit status, fatal logs, crash reports, forbidden behavior, runtime identity, incremental reuse, timeout classification, and cleanup.

The goal is complete only when current evidence proves required L0 through L5 gates, both product matrices, app-visible Linux userspace output, clean safety evidence, and confirmed TestFlight publication for one exact build.
