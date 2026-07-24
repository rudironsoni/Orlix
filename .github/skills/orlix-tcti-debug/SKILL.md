---
name: orlix-tcti-debug
description: >-
  Orlix TCTI debugging with native LLDB and kernel-owned C proof. Use for
  crashes, watchpoints, disassembly, ELF, Mach-O, instruction encodings,
  symbols, sections, relocations, and TCTI runtime corruption.
---
# Orlix TCTI Debug

Use native LLDB to locate the first incorrect state transition. Keep durable
diagnostics and proof in the owning kernel subsystem.

## Workflow

1. Reproduce the defect through its smallest owning suite and record the exact
   failing state. This step is complete only when the failure is repeatable or
   its nondeterminism is explicitly bounded.
2. Inspect the live process with native LLDB breakpoints, conditional
   breakpoints, hardware watchpoints, stepping, variables, registers, memory,
   expressions, disassembly, symbol lookup, and command-only stop hooks. This
   step is complete when the first incorrect transition and its writer are
   identified.
3. If native LLDB cannot expose the required state, add temporary C
   instrumentation inside the owning Orlix subsystem, guarded by its existing
   debug configuration and removable after diagnosis. This step is complete
   when structured C state identifies the transition LLDB could not observe.
4. Fix the root cause in its owning layer, then remove temporary instrumentation
   that is not a durable invariant. This step is complete when the original
   reproducer remains green without diagnostic scaffolding.
5. Add a KUnit regression for TCTI internals. Add or update kselftest when the
   defect changes Linux-visible behavior. This step is complete only when the
   owning tests fail on the defect and pass on the correction.

## Ownership

- TCTI invariants and runtime assertions belong in C under `arch/orlix`.
- TCTI regression tests belong in KUnit.
- Linux-visible behavior belongs in kselftest.
- LLDB and LLVM binary tools observe execution. Their output does not replace
  behavioral proof.

## Native LLDB Surface

- Breakpoints and conditional breakpoints.
- Watchpoints and hardware watchpoints.
- `thread step-in`, `thread step-over`, and `thread until`.
- `frame variable`, `register read`, `memory read`, and `expression`.
- `disassemble`, `image lookup`, and command-only `target stop-hook`.

## Hard Guardrails

- Never use LLDB Python scripts, `breakpoint command add --python-function`, the
  LLDB Python API, Python callbacks, external-language callbacks, helper
  processes, or helper scripts in the debugging session.
- Never move debugging logic outside the repository because it is temporary.
- Never guess instruction encodings or treat debugger observations as final
  correctness proof.

## Binary Tools

- `file <path>`
- `xcrun llvm-objdump -d <path>`
- `xcrun llvm-readelf -a <path>`
- Externally configured LLDB MCP `lldb_command`
