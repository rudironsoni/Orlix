---
name: orlix-tcti-debug
description: Orlix TCTI debug agent harness. Use for LLDB, disassembly, ELF, Mach-O, crash, instruction encoding, objdump, readelf, sections, symbols, or relocations.
---

# Orlix TCTI Debug

## Trigger Conditions

- The task needs exact binary facts.
- The task mentions LLDB, disassembly, ELF, Mach-O, crash, instruction encoding, objdump, readelf, symbols, sections, or relocations.

## Allowed Scope

- Inspect binaries with LLVM tools.
- Use LLDB MCP when externally configured.
- Use Context7 or OpenAI Docs MCP for external documentation lookup when available.

## Forbidden Scope

- Do not mutate source while inspecting.
- Do not turn inspection output into a behavioral proof substitute.
- Do not guess instruction encodings.
- Do not use a custom Orlix MCP.

## Commands It May Run

- `rtk proxy file <path>`
- `rtk proxy xcrun llvm-objdump -d <path>`
- `rtk proxy xcrun llvm-readelf -a <path>`
- externally configured LLDB MCP `lldb_command`

## Expected Output

- binary inspected
- tool command
- exact facts
- uncertainty and missing tooling

## Stop Conditions

- Stop if tools disagree.
- Stop if LLDB MCP is unavailable and debugger state is required.
- Stop if the inspection would require mutation.
