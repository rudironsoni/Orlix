---
name: orlix-tcti-next-step
description: Orlix TCTI next-step agent harness. Use when continuing TCTI work, running the harness, choosing the next gate, or coordinating planner and safety review.
---

# Orlix TCTI Next Step

## Trigger Conditions

- The user asks to continue TCTI work.
- The user says "run the harness" or asks "what next".
- A TCTI implementation task is proposed without a clear gate.
- A future prompt asks to execute the next eligible TCTI gate.
- The user invokes `/goal` or asks for autonomous TCTI goal continuation.

## Allowed Scope

- Read `AGENTS.md`, `PLAN.md`, `IMPLEMENT.md`, and TCTI reports.
- Read `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`.
- Read `.agents/skills/orlix-tcti-next-step/references/environment-policy.json`.
- Run agent-neutral harness checks and TCTI no-phone status checks.
- Generate `Build/AgentHarness/orlix-tcti/status.json`.
- Generate `Build/AgentHarness/orlix-tcti/next-task.json` and `Build/AgentHarness/orlix-tcti/next-task.md`.
- Expose simulator eligibility explicitly, including the pinned simulator ID/name from environment policy or `ORLIX_TCTI_REQUIRED_SIMULATOR_*` overrides and whether simulator gates are complete.
- Expose full simulator readiness through `simulator_readiness_gate_ids`, `simulator_readiness_missing_gate_ids`, and `physical_device_blockers`.
- Expose selected gate proof-tier metadata: `selected_gate_proof_tier`, `selected_gate_acceptance_weight`, `selected_gate_real_stack_required`, and `selected_gate_can_claim_runtime_readiness`.
- Spawn or simulate planner, safety reviewer, LLVM inspector, oracle engineer, reducer, and release-gate reviewer roles.
- Produce the next safe task, scope, forbidden work, verification gates, reducer requirements, and commit message.

## Forbidden Scope

- Do not implement TCTI runtime features directly.
- Do not run physical-device gates.
- Do not add production assembly or gadget dispatch.
- Do not bypass planner and safety reviewer scope for TCTI work.
- Do not hardcode a one-off human prompt as the next gate.
- Do not treat golden ELF, switch-debug, reducer, or switch-vs-gadget seed probes as Linux runtime readiness.
- Do not skip the pinned simulator readiness ladder before physical-device work. The ladder is first syscall, runtime stability, Linux console usability, static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader support, signals, VFS completeness, and full Linux runtime readiness.

## Commands It May Run

- `rtk proxy make agent-status AREA=orlix-tcti`
- `rtk proxy make agent-next AREA=orlix-tcti`
- `rtk proxy make agent-task-envelope-check AREA=orlix-tcti`
- `rtk proxy make agent-goal AREA=orlix-tcti`
- `rtk proxy make agent-harness-check`
- `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`
- `rtk proxy make tcti-gate TARGET=tcti-report-schema-check`
- `rtk proxy make tcti-gate TARGET=tcti-golden-elf`
- `rtk git status --short`

## Expected Output

- status JSON path
- next-task JSON path
- next-task Markdown path
- selected next gate
- selected gate command
- selected gate result classification and action policy
- selected gate proof tier
- selected gate acceptance weight
- whether the selected gate requires the real stack
- whether the selected gate can claim runtime readiness
- prerequisite report paths
- allowed scope
- forbidden scope
- required validation commands
- reducer requirements
- required subagents or skills
- commit message
- stop conditions
- for `agent-goal`, a structured loop handoff with iteration count, command count, last selected gate, next selected gate, selected action-policy fields, and readiness truth

## Stop Conditions

- Stop if `agent-harness-check` fails.
- Stop if `agent-task-envelope-check` fails.
- Stop if `agent-goal` reaches `must_stop=true`, `runtime_patch_allowed=true`, `harness_patch_allowed=true`, `goal_complete=true`, `MAX_ITERATIONS`, `MAX_COMMANDS`, tracked source changes, an environment-only failure, a forbidden behavior violation, or selected-command process failure.
- Stop if `tcti-plan-consistency` fails.
- Stop if a seed/golden gate is marked runtime-ready or readiness-eligible.
- Stop if a Coreutils/package gate depends only on golden ELF or seed probes.
- Stop if an OCI gate lacks OrlixOS rootfs/session proof.
- Stop if the next task needs production assembly or gadget work before no-phone prerequisites pass.
- Stop if a physical-device gate is selected before the pinned simulator has current passing reports for first syscall, runtime stability, Linux console usability, static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader support, signals, VFS completeness, and full Linux runtime readiness.
- Stop if physical-device opt-in variables are treated as an override for missing, stale, failing, evidence-only, or emergency-override simulator readiness reports.
- Stop if a physical-device gate is selected without explicit `ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1` or `ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED=1` after the full pinned simulator readiness ladder has passed.
- Stop if a simulator-runtime gate does not target the pinned Orlix-iPhone-15-Pro-Max simulator.

## Selector And Goal Loop

`agent-next` is a one-shot selector. It refreshes `status.json`, selects one gate, and writes `Build/AgentHarness/orlix-tcti/next-task.json`.

`agent-goal` is the autonomous loop. It runs `.agents/skills/orlix-tcti-next-step/scripts/goal-loop`, which runs `agent-status`, `agent-next`, and `agent-task-envelope-check`, reads the selected-gate action policy from `next-task.json`, and executes the exact `selected_gate_command` only when:

- `continue_refresh_allowed=true`
- `must_stop=false`
- `runtime_patch_allowed=false`
- `harness_patch_allowed=false`

`missing_generated_artifact` may authorize continuation only when the selected command is a supported no-phone `tcti-gate` command or the pinned-simulator `runtime-validation` command, all prerequisites are satisfied, and the gate is not physical-device work. Missing artifacts without a known safe generator remain stop conditions.

After every selected command, `agent-goal` regenerates status, next-task, and the task envelope before deciding again. It does not infer action from prose or from `status=fail`; it only follows the classifier and action-policy fields in `next-task.json`.

The loop regenerates status and next-task with `ORLIX_TCTI_HARNESS_QUIET=1` so autonomous runs emit compact iteration and handoff records instead of streaming the full roadmap. Direct status and next commands remain human-readable.

Use `DRY_RUN=1` to inspect what the loop would execute without running selected commands. Use `MAX_ITERATIONS` and `MAX_COMMANDS` to bound work; both default to 25.
