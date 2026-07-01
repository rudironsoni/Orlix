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

Do not add Codex-specific compatibility aliases for these targets.

## Workflow

Use `$orlix-tcti-next-step` for TCTI continuation. The skill routes through planner and safety review before implementation. The oracle skill owns no-phone switch-debug and golden ELF work. The debug skill owns LLVM and LLDB inspection. The reproducer skill owns reducer replay. The safety skill owns App Store, x18, JIT, MAP_JIT, RWX, PROT_EXEC, HostAdapter, defconfig, and generated-tree checks.

Do not run physical-device TCTI work unless runtime-validation preflight permits it. Do not add production TCTI assembly or gadget dispatch until switch-debug oracle coverage exists for the target and safety reports pass.
