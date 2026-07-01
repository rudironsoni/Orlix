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

Do not add Codex-specific compatibility aliases for these targets.

## Workflow

Use `$orlix-tcti-next-step` for TCTI continuation. The skill runs `agent-status`, `agent-next`, and `agent-task-envelope-check` so the repo selects the next eligible gate from `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json` and current reports. The generated task envelope under `Build/AgentHarness/orlix-tcti/` is the scope contract.

The standard autonomous workflow is:

1. `orlix-tcti-next-step` runs `agent-status` and `agent-next`.
2. `tcti-planner` reviews the task envelope.
3. `tcti-safety-reviewer` reviews forbidden scope.
4. The relevant implementer subagent works only inside allowed scope.
5. `tcti-test-reducer` handles failures before production changes.
6. `tcti-release-gate-reviewer` decides whether the checkpoint advances readiness.

The oracle skill owns no-phone switch-debug and golden ELF work. The debug skill owns LLVM and LLDB inspection. The reproducer skill owns reducer replay. The safety skill owns App Store, x18, JIT, MAP_JIT, RWX, PROT_EXEC, HostAdapter, defconfig, and generated-tree checks.

Do not run physical-device TCTI work unless runtime-validation preflight permits it. Do not add production TCTI assembly or gadget dispatch until switch-debug oracle coverage exists for the target and safety reports pass.
