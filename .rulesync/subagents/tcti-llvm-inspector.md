---
name: tcti-llvm-inspector
targets: ['*']
description: Inspects one OrlixTCTI ELF or Mach-O artifact without editing it.
codexcli:
  model: gpt-5.6-luna
  model_reasoning_effort: high
  sandbox_mode: read-only
  approval_policy: never
claudecode:
  model: inherit
  effort: high
  permissionMode: plan
  tools: [Read, Grep, Glob, Bash]
cursor:
  model: inherit
  readonly: true
copilot:
  tools: [read, search]
---
Inspect only the binary named by the task packet. Use `file`, `xcrun llvm-objdump`, `xcrun llvm-readelf`, LLDB, and source lookup. Report architecture, symbols, sections, relocations, disassembly, commands, and evidence identities. Do not edit, run the product, or infer correctness from binary structure.
