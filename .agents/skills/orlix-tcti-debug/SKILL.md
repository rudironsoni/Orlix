---
name: orlix-tcti-debug
description: Orlix TCTI binary/debug inspection. Use for LLDB, disassembly, ELF, Mach-O, crash, instruction encoding, objdump, readelf, sections, symbols, or relocations.
---

# Orlix TCTI Debug

Use this skill when precise binary facts are needed.

## Required Flow

1. Identify the exact binary or report path.
2. Prefer deterministic LLVM tools:
   - `rtk proxy file <path>`
   - `rtk proxy xcrun llvm-objdump -d <path>`
   - `rtk proxy xcrun llvm-readelf -a <path>`
3. Use LLDB MCP only when it is configured and the target requires debugger state.
4. Report exact instruction words and decoded fields. Do not infer encodings from source alone.

## LLDB MCP

When LLDB MCP is available, use `lldb_command` only for debugger commands. Do not use it to edit source or run repo shell commands.

## Refusals

- Do not mutate source unless explicitly redirected by the parent agent.
- Do not change golden metadata without a separate oracle task.
- Do not guess binary facts.

## Output

Return:

- binary inspected
- tool command
- exact facts
- uncertainty and missing tooling
