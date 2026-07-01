# MCP Policy

## Policy

Orlix does not create ad hoc homemade MCP servers for repo-local Make commands, report readers, or reducer readers.

Repo-local workflows are implemented as skills under `.agents/skills/`. Skill scripts live under `.agents/skills/<skill-name>/scripts/`.

MCP is reserved for ready, proven external tools, such as:

- LLDB MCP
- Context7 MCP
- OpenAI Docs MCP
- GitHub or issue-tracker MCP when externally provided and useful

## Custom MCP Review Bar

A future custom MCP requires a separate design review and must prove:

- it cannot be safely represented as a skill or Make target
- it serves multiple agents or external clients
- it has a stable schema
- it has explicit security boundaries
- it is tested
- it does not expose arbitrary shell execution

No custom Orlix MCP is active.
