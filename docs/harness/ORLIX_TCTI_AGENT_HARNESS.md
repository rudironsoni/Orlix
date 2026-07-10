# Orlix TCTI Agent Harness

The Orlix TCTI agent harness is agent-neutral. Codex is one adapter.

## Repository Harness

Reusable repo-local workflow logic lives in skills:

- `.agents/skills/orlix-tcti-next-step/`
- `.agents/skills/orlix-tcti-status/`
- `.agents/skills/orlix-tcti-report-reader/`
- `.agents/skills/orlix-tcti-reproducer/`
- `.agents/skills/orlix-tcti-golden-elf/`
- `.agents/skills/orlix-tcti-safety/`
- `.agents/skills/orlix-tcti-plan-consistency/`
- `.agents/skills/orlix-tcti-debug/`

Scripts live inside skills under `.agents/skills/<skill-name>/scripts/`.

## Codex Adapter

Codex-specific integration remains under `.codex/`:

- `.codex/config.toml`
- `.codex/hooks.json`
- `.codex/hooks/`
- `.codex/subagents/`

Codex hooks call skill-local scripts. They should not become the canonical project workflow.
Sandbox mode, approval policy, secrets, and machine-local MCP credentials belong in `~/.codex/config.toml`, not in the committed repo adapter.

## MCP

MCP is limited to external, proven tools:

- OpenAI Docs MCP for OpenAI and Codex documentation
- Context7 MCP for external developer documentation
- LLDB MCP for debugger interaction

Repo-local TCTI report reading, reducer replay, golden ELF validation, status, and safety checks are skills and Make targets, not MCP tools.

## Canonical Make Targets

Use agent-neutral targets:

- `make agent-harness-check`
- `make agent-hooks-check`
- `make agent-skills-check`
- `make agent-subagents-check`
- `make agent-mcp-check`
- `make agent-status AREA=orlix-tcti`
- `make agent-next AREA=orlix-tcti`
- `make agent-task-envelope-check AREA=orlix-tcti`
- `make agent-goal AREA=orlix-tcti`

Do not add Codex-specific compatibility aliases for these targets.

## Workflow

Use `$orlix-tcti-next-step` for TCTI continuation. `agent-status`, `agent-next`, and `agent-task-envelope-check` are one-shot selector and envelope commands. They select the next eligible gate from `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json` and current reports, then write the task envelope under `Build/AgentHarness/orlix-tcti/`.

Use `agent-goal` for autonomous goal execution. It is the bounded loop that repeatedly runs the one-shot selector/envelope commands, consumes the selected-gate action policy in `next-task.json`, executes the exact selected command when refresh continuation is allowed, regenerates the envelope, and stops on policy stop or budget exhaustion.

Inside `agent-goal`, status and next-task regeneration is quiet: JSON and Markdown artifacts are still rewritten and validated, but the full human-readable roadmap is not printed on every iteration. Direct `make agent-status` and `make agent-next` commands retain their normal human-readable output.

## Report Freshness

Report `git_sha` is provenance. It is not, by itself, the execution freshness key for selected-gate reports.

`agent-status` recomputes harness/status truth at current HEAD every run. For report execution freshness, the selector compares `report.git_sha` to current HEAD and, when they differ, classifies `git diff --name-only report.git_sha..HEAD` against the selected gate invalidation scope. Reports remain execution-fresh when the only changed paths are docs, IMPLEMENT checkpoints, Codex adapter instructions, or harness/status presentation files that cannot affect the gate execution result. Reports become execution-stale only when changed paths intersect the selected gate's execution inputs.

`status.json` includes `execution_freshness` evidence on report facts when semantic freshness is available:

- `report_git_sha`
- `current_git_sha`
- `execution_fresh`
- `status_recomputed`
- `changed_paths_since_report`
- `ignored_non_execution_paths`
- `invalidating_paths`
- `reason`

An execution-fresh old-SHA report must not be selected as `stale_proof_refresh` solely because HEAD changed. An execution-stale report is selected as `stale_proof_refresh` and may be refreshed by `agent-goal` only through the selected-gate action policy.

## Proof Tiers

The roadmap is tiered so the harness manages proof toward real Linux userspace instead of treating golden ELF probes as acceptance. Every roadmap gate declares:

- `proof_tier`: `seed`, `rail`, `safety`, `kernel`, `kselftest`, `mlibc`, `mlibc-uapi`, `shell`, `coreutils`, `oci`, `simulator`, `device`, or `release`
- `acceptance_weight`: `probe`, `blocker`, `readiness`, or `release`
- `real_stack_required`
- `can_claim_runtime_readiness`

Golden ELF, switch-debug, reducer, and switch-vs-gadget gates are `proof_tier=seed`, `acceptance_weight=probe`, `real_stack_required=false`, and `can_claim_runtime_readiness=false`. They are microscopes for decoder bring-up, exact regressions, no-phone reducers, and switch-vs-gadget differentials. They are not proof that Linux userspace works.

Runtime readiness must move through the real stack: OrlixKernel syscall/exec/fault/wait/console behavior, kselftest or kernel-interface subsets, real OrlixMLibC-linked binaries, shell behavior, real Coreutils or upstream package commands, OrlixOS OCI/rootfs/session materialization, app-hosted simulator execution, device certification, and only then release/default-flip gates.

`agent-task-envelope-check` and `agent-harness-check` enforce this contract. They reject seed gates that claim runtime readiness, Coreutils gates backed only by seed probes, OCI gates without OrlixOS rootfs/session proof, device gates without simulator and real-stack prerequisites, and passing report fixtures without proof-tier metadata.

The standard autonomous workflow is:

1. `orlix-tcti-next-step` runs `agent-status` and `agent-next`.
2. `tcti-planner` reviews the task envelope.
3. `tcti-safety-reviewer` reviews forbidden scope.
4. The relevant implementer subagent works only inside allowed scope.
5. `tcti-test-reducer` handles failures before production changes.
6. `tcti-release-gate-reviewer` decides whether the checkpoint advances readiness.

If `agent-next` selects a gate whose current report is `status=todo`, the manager assigns work to implement the selected gate body or the missing real workload hook. Rerunning the TODO command is not progress, and a TODO report is not runtime proof. The TODO report exists only to make the selected command executable, scoped, and honest until the real gate can run.

The oracle skill owns no-phone switch-debug and golden ELF work. That work can unlock implementation and reduce failures, but it must not become the release gate. The debug skill owns LLVM and LLDB inspection. The reproducer skill owns reducer replay. The safety skill owns App Store, x18, JIT, MAP_JIT, RWX, PROT_EXEC, HostAdapter, defconfig, and generated-tree checks.

Do not run physical-device TCTI work unless runtime-validation preflight permits it. Do not add production TCTI assembly or gadget dispatch until switch-debug oracle coverage exists for the target and safety reports pass.

## Selected Gate Result Policy

`agent-next` must emit a post-run action policy for the selected gate in
`Build/AgentHarness/orlix-tcti/next-task.json` and `next-task.md`.
`agent-task-envelope-check` must recompute that policy from current
`status.json` and reject stale or missing policy fields.

The selected gate policy classifies the latest generated evidence before any
agent decides whether to refresh proof, repair harness evidence, stop, or patch
runtime code. The envelope exposes:

- `selected_gate_result_classification`
- `runtime_patch_allowed`
- `harness_patch_allowed`
- `continue_refresh_allowed`
- `must_stop`
- `required_next_action`
- `result_classification_reason`
- `owning_layer`

Runtime code edits require `runtime_patch_allowed=true` from the current
envelope. Stale proof refresh, missing generated artifacts, rail evidence
contract bugs, proof-tier/report metadata drift, environment-only failures,
and forbidden behavior violations must not be treated as permission to patch
OrlixKernel/TCTI runtime code.

`agent-goal` continues only when all of these selected-gate policy fields are true for continuation:

- `continue_refresh_allowed=true`
- `must_stop=false`
- `runtime_patch_allowed=false`
- `harness_patch_allowed=false`

The loop stops and prints a structured handoff when the policy says to stop, when runtime or harness patching is required, when forbidden behavior or environment-only failure appears, when tracked source changes appear, or when `MAX_ITERATIONS` or `MAX_COMMANDS` is reached. If a selected command fails at process level, the loop first regenerates `agent-status`, `agent-next`, and `agent-task-envelope-check`, reloads the refreshed selected-gate classifier, and then stops with the classified action policy for that failed gate. The handoff must include readiness truth for full TCTI completion, global runtime readiness, package readiness, release-gate eligibility, physical-device allowance, simulator gate completion, and the app-visible `ORLIX-USERLAND-TCTI-OK` marker. The loop does not patch runtime code unless `runtime_patch_allowed=true`; it does not patch harness code unless `harness_patch_allowed=true`.

For `missing_generated_artifact`, the classifier may continue only through a supported no-phone `tcti-gate` command or the exact pinned-simulator `runtime-validation` command when prerequisites are satisfied. This allows `/goal` to generate a selected proof artifact without interpreting missing generated state as runtime implementation authority. Missing artifacts with no known safe generator still require a stop.
