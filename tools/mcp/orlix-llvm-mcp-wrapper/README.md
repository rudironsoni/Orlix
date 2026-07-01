# Orlix LLVM MCP Wrapper

This wrapper connects Codex to LLDB MCP when the local LLVM installation provides `lldb-mcp`.

LLDB setup:

```text
(lldb) protocol-server start MCP listen://localhost:59999
```

`lldb-mcp` bridges stdio clients to LLDB's MCP server. The LLDB MCP tool exposed to Codex is `lldb_command`, which runs LLDB command-interpreter commands against the selected debugger.

Use this only for debugger inspection. Do not use LLDB MCP to run repository shell commands or mutate source.
