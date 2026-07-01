# Orlix TCTI Codex Harness

This harness makes Orlix TCTI work run through explicit planning, safety review, no-phone verification, reducer discipline, and binary inspection before device work.

## Codex Configuration

Project-scoped Codex settings live in `.codex/config.toml`. User secrets and machine-local credentials must stay in `~/.codex/config.toml`.

Configured project MCP entries:

- `openaiDeveloperDocs`: public OpenAI developer documentation MCP at `https://developers.openai.com/mcp`.
- `context7`: disabled by default. Enable in user config and provide `CONTEXT7_API_KEY` locally if needed.
- `lldb`: disabled by default. Requires local `lldb-mcp` and a running or launchable LLDB MCP server.
- `orlix-tcti`: disabled by default. Project-local domain adapter under `tools/mcp/orlix-tcti-mcp/`.

## LLDB MCP

LLDB MCP setup follows the official LLDB MCP flow:

```text
(lldb) protocol-server start MCP listen://localhost:59999
```

`lldb-mcp` bridges stdio clients to LLDB's MCP server and exposes `lldb_command`.

## Context7 MCP

Context7 can be configured with the remote endpoint:

```toml
[mcp_servers.context7]
url = "https://mcp.context7.com/mcp"
env_http_headers = { "CONTEXT7_API_KEY" = "CONTEXT7_API_KEY" }
```

Keep the API key in the user environment or `~/.codex/config.toml`, not in the repo.

## Orlix TCTI MCP

The Orlix MCP server exposes only domain tools:

- `tcti_status`
- `tcti_next`
- `tcti_report_read`
- `tcti_reproducer_read`
- `tcti_golden_list`
- `tcti_golden_validate`
- `tcti_safety_audit`
- `tcti_plan_consistency`

It does not expose arbitrary shell execution.

## Workflow

Use `$orlix-tcti-next-step` for TCTI continuation. The skill must route through planner and safety review before implementation. The oracle engineer may implement no-phone switch-debug and golden ELF work only inside the approved scope. The LLVM inspector reports binary facts. The reducer converts failures into replayable fixtures. The release gate reviewer decides whether evidence can advance readiness.
