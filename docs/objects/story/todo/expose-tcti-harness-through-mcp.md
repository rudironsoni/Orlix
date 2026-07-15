---
type: story
tags:
  - story
  - tcti-mcp
updated: 2026-07-15
status: todo
summary: "Expose stable TCTI harness state through a read-oriented MCP interface."
story_of:
  - "[TCTI MCP](../../epic/todo/tcti-mcp.md)"
has_task:
  - "[Stabilize the TCTI report contract](../../task/todo/stabilize-tcti-report-contract.md)"
  - "[Define the TCTI MCP read interface](../../task/todo/define-tcti-mcp-read-interface.md)"
---

# Expose the TCTI harness through MCP

As an Orlix agent, I want a stable read-oriented interface to TCTI task and report state so workflow tools can query the authoritative harness without parsing prose or inventing execution state.

Implementation begins only after the underlying report and task-envelope schemas are stable enough to support a durable interface.
