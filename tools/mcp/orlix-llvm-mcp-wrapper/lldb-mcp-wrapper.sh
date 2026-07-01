#!/usr/bin/env sh
set -eu

if ! command -v lldb-mcp >/dev/null 2>&1; then
  printf '%s\n' "lldb-mcp is not installed or not on PATH." >&2
  printf '%s\n' "Install LLVM LLDB MCP support, then start LLDB MCP with:" >&2
  printf '%s\n' "  (lldb) protocol-server start MCP listen://localhost:59999" >&2
  exit 127
fi

exec lldb-mcp "$@"
