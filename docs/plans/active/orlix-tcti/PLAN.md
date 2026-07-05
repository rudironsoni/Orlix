# PLAN.md

## Task

Revise and implement ADR 0022 so Orlix uses Orlix TCTI as the first physical-iPhone userspace execution backend for ordinary unmodified AArch64 Linux ELF binaries.

This is not an iSH runtime port and not a demo. Linux is already the runtime:

- `OrlixKernel` is upstream Linux plus `arch/orlix`.
- `OrlixMLibC` is libc for Orlix Linux userspace.
- `OrlixOS` is the delivered OS Kit.
- `OrlixHostAdapter` is private iOS/Darwin mediation only.

TCTI only executes guest AArch64 EL0 instructions until Linux needs control again.

## Success Criteria

- ADR 0022 no longer says native host execution is the required initial backend.
- ADR 0022 preserves Linux ELF, Linux UAPI, syscall numbers, errno behavior, VFS, fd tables, signals, wait/reaping, `execve`, and process semantics in OrlixKernel.
- Orlix TCTI exists under `arch/orlix`, not HostAdapter.
- TCTI does not decode Linux syscall policy, model Linux processes, or own VFS/fd/signal/wait/exec semantics.
- Guest ELF text remains host data. No guest ELF text path requests `vm_protect(... EXECUTE ...)`, guest-text `mmap(... PROT_EXEC ...)`, JIT, MAP_JIT, RWX, or generated executable memory.
- Neither this plan nor the full TCTI objective can be marked complete by documentation, harness rails, or no-phone seed proofs alone. Completion requires local no-phone test targets, machine-readable JSON reports, checked-in golden artifacts, reducer artifacts, static audits, gadget readiness where required, and physical-device runtime evidence.
- The repo can select the next eligible TCTI gate without a human naming it. `make agent-status AREA=orlix-tcti`, `make agent-next AREA=orlix-tcti`, and `make agent-task-envelope-check AREA=orlix-tcti` must produce and validate a machine-readable next-task envelope from current reports and the skill-owned roadmap.
- Simulator validation is mandatory before physical iPhone validation. The simulator must run on the dedicated `Orlix-iPhone-15-Pro-Max` simulator and must prove the full simulator ladder: first TCTI `svc #0` marker, post-launch TCTI runtime stability, Linux console usability, static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader support, signals, VFS completeness, and full Linux runtime readiness. Every report must be current, passing, non-preflight, non-emergency-override, and free of kernel panic, init death, user fault, BUG, Oops, SIGSEGV, fatal error, or crash markers. Direct physical `runtime-validation` preflight must enforce the same full simulator ladder, not only the agent-next selector.
- Physical iPhone gate proves static `/init` reaches real `svc #0`, enters `orlix_syscall_dispatch`, writes one Linux console line, and emits HostAdapter console mirror evidence.
- Performance claims include exact workload, device, build configuration, command, baseline, counters, wall-clock result, JSON report, and Markdown report.

## Completion Claim Boundary

Current status: full TCTI is incomplete.

The current harness and no-phone proofs are scaffolding and early oracle evidence only. They do not prove that Orlix TCTI is the physical-iPhone userspace execution backend. They do not prove runtime readiness. They do not prove package readiness. They do not prove performance.

Agents may mark only the exact scoped checkpoint they just verified. They must not mark the whole `orlix-tcti` objective, this plan, ADR 0022 implementation, product readiness, or physical-iPhone TCTI support complete while any of these are true:

- the next selected gate is still a no-phone switch-debug gate such as `switch-init-003-stack`
- `make agent-status AREA=orlix-tcti` reports `physical_device_allowed=false`
- release or readiness eligibility is false
- any required TCTI report is missing, `todo`, `fail`, `error`, `skipped`, or `evidence`
- gadget prerequisites are incomplete for a gadget or physical-device claim
- any pinned simulator ladder report has not produced a current passing JSON report, including first syscall, runtime stability, Linux console usability, static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader support, signals, VFS completeness, and full Linux runtime readiness
- `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall` has not produced a passing JSON report
- the physical-device report does not prove real `/init` reaches `svc #0`, enters `orlix_syscall_dispatch`, writes a Linux console line, and has all forbidden-behavior fields false

Full TCTI completion requires a final evidence checkpoint in `IMPLEMENT.md` with exact commands, report paths, reducer status, physical-device evidence, and explicit confirmation that the product defconfig flip is allowed by the gates. Anything less is partial progress.

## Agent Harness Autonomy

TCTI work must start from the agent-neutral harness, not from a one-off prompt that names the next gate.

Canonical entry points:

- `make agent-status AREA=orlix-tcti`
- `make agent-next AREA=orlix-tcti`
- `make agent-task-envelope-check AREA=orlix-tcti`

These targets delegate only to skill-local scripts under:

- `.agents/skills/orlix-tcti-next-step/scripts/status`
- `.agents/skills/orlix-tcti-next-step/scripts/next`
- `.agents/skills/orlix-tcti-next-step/scripts/task-envelope-check`
- `.agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift`

The roadmap is repo-local skill reference data, not an MCP server:

- `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`

The harness writes:

- `Build/AgentHarness/orlix-tcti/status.json`
- `Build/AgentHarness/orlix-tcti/next-task.json`
- `Build/AgentHarness/orlix-tcti/next-task.md`

The next-task envelope is the implementation scope contract. It must include:

- selected gate id
- selected gate command
- selected gate proof tier
- selected gate acceptance weight
- whether the selected gate requires the real stack
- whether the selected gate can claim runtime readiness
- prerequisite gates and report paths
- why the gate was selected
- allowed scope
- forbidden scope
- required validation commands
- expected report paths
- reducer requirements
- required subagents or skills
- commit message
- stop conditions

The envelope validator must fail if:

- selected gate is not present in the roadmap
- prerequisites are not satisfied
- forbidden scope is empty
- validation commands are missing
- expected report paths are missing
- a seed or golden gate claims runtime readiness
- a Coreutils or upstream-package gate depends only on golden ELF or seed probes
- an OCI gate lacks OrlixOS rootfs/session proof
- a physical-device gate lacks simulator and real-stack prerequisites
- a product-default flip lacks real-stack and device proof
- a passing report fixture lacks proof-tier metadata
- a physical-device gate is selected while no-phone gates are incomplete
- a physical-device gate is selected while the explicit pinned simulator readiness list has any missing, stale, failing, evidence-only, preflight-only, or emergency-override report
- a gadget gate is selected before switch-debug oracle coverage exists
- custom Orlix MCP is referenced
- `tools/agent` is referenced or exists
- scripts live outside `.agents/skills/<skill-name>/scripts/`
- JSON is not machine-parseable

The status and next-task JSON must carry the full simulator-readiness contract:

- `simulator_readiness_gate_ids`
- `simulator_readiness_missing_gate_ids`
- `physical_device_blockers`

Those fields are not advisory. If `simulator_readiness_missing_gate_ids` is non-empty, no phone work may be selected, requested, or treated as evidence of progress toward release readiness.

Standard autonomous workflow:

1. `orlix-tcti-next-step` runs `agent-status` and `agent-next`.
2. `tcti-planner` reviews the task envelope.
3. `tcti-safety-reviewer` reviews forbidden scope.
4. The relevant implementer subagent works only inside allowed scope.
5. `tcti-test-reducer` handles failures before production changes.
6. `tcti-release-gate-reviewer` confirms whether the checkpoint advances readiness.

Current roadmap state after the latest no-phone proofs and simulator policy correction:

- The no-phone and simulator prerequisites already proven by reports remain prerequisites, not final completion.
- `simulator-tcti-init-first-syscall` proves only that the simulator build reaches the first TCTI `svc #0` marker.
- `simulator-tcti-runtime-stability` is mandatory after the first-syscall marker and before any phone gate. It fails on fatal post-launch TCTI runtime evidence and emits the fatal log artifact for reduction.
- `simulator-tcti-linux-console-usability` is mandatory after simulator runtime stability and before any phone gate. It runs `runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-console-write` on `Orlix-iPhone-15-Pro-Max` and requires a current passing JSON report plus the console marker artifact.
- Static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader support, signals, VFS completeness, and full Linux runtime readiness are also mandatory pinned-simulator gates before any phone work.
- Physical-device gates remain blocked while any pinned simulator ladder report fails or is missing, even if the first-syscall marker, stability, or console marker report passes.
- Gadget and production TCTI work remain blocked unless the selected harness envelope explicitly allows them.
- This status is proof of sequencing only. It is not a completion claim for TCTI, product readiness, or physical-device support.

No custom Orlix MCP may be introduced for these workflows. Repo-local workflow logic belongs in `.agents/skills`; `.codex` is only an adapter; MCP is reserved for external/proven tools such as LLDB MCP, Context7, OpenAI Docs MCP, or externally configured issue-tracker and GitHub MCP.

## TCTI Proof Tiers

The TCTI harness is an engineering manager over proof tiers, not a second runtime oracle. The roadmap must classify gates with:

- `proof_tier`: `seed`, `kernel`, `kselftest`, `mlibc`, `mlibc-uapi`, `shell`, `coreutils`, `oci`, `simulator`, `device`, or `release`
- `acceptance_weight`: `probe`, `blocker`, `readiness`, or `release`
- `real_stack_required`
- `can_claim_runtime_readiness`

Tier meaning:

- tier 0, `seed`: synthetic seed probes, golden ELF, switch-debug, reducers, and switch-vs-gadget differentials
- tier 1, `kernel`: OrlixKernel/TCTI syscall, exec, fault, wait, reaping, and console probes
- tier 2, `kselftest`: Linux kselftest or kernel-interface subset proof
- tier 3, `mlibc`: real OrlixMLibC libc/sysdeps proof
- tier 4, `mlibc-uapi`: OrlixMLibC-linked syscall/UAPI programs
- tier 5, `shell`: POSIX shell environment proof
- tier 6, `coreutils`: real Coreutils or upstream package command proof
- tier 7, `oci`: OrlixOS OCI/rootfs/session materialization and command proof
- tier 8, `simulator`: app-hosted simulator proof through runtime validation
- tier 9, `device`: physical-device certification
- tier 10, `release`: product default flip and release gates

Golden ELF gates remain useful, but only as microscopes, reducers, and trace-led seed probes. They can unlock implementation prerequisites. They cannot mark Orlix Linux-usable, package-ready, simulator-ready, device-ready, or release-ready.

The real acceptance path is Linux, libc, packages, OCI, app-hosted simulator, device proof, forbidden-behavior checks, and product default gating. Do not call TCTI complete until the kernel/kselftest subset, OrlixMLibC real test subset, OrlixMLibC-linked syscall/UAPI programs, shell smoke, Coreutils subset, OCI rootfs materialization and command execution, app-hosted simulator proof, physical first-syscall proof, physical console proof, forbidden behavior false, and product defconfig flip gates pass.

## Physical Evidence Being Corrected

Physical iPhone evidence showed Linux reaching `/init`, then failing when the native hosted execution path attempted to make copied anonymous Linux ELF text executable and iOS returned `KERN_PROTECTION_FAILURE`.

This invalidates the old native-user-text assumption only. It does not invalidate Linux ELF, Linux `execve`, Linux `binfmt_elf`, OrlixMLibC, or OrlixKernel ownership of Linux semantics.

## Ownership And Boundaries

Owning layer:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix`

Why this layer owns it:

- TCTI executes the current Linux task userspace instructions using Linux task, mm, syscall, fault, signal, and scheduling state.

HostAdapter may provide only private host mechanics:

- memory backing
- console mirror
- timers
- lifecycle observation
- device logging
- resource registration
- block/file/image backing for Linux drivers

HostAdapter must not:

- decode guest AArch64 instructions
- decode Linux syscall numbers
- own Linux process semantics
- own Linux VFS, fd, signal, wait, or exec behavior
- implement Linux syscall policy
- own guest page-table semantics
- know about TCTI gadget programs

Generated-tree policy:

- Generated upstream/build trees may be read for diagnosis.
- Fixes must land in durable Orlix overlay inputs.
- Do not edit `Build/OrlixKernel/src/linux-*-port` or other generated source trees to make tests pass.

## Exact Orlix Files Inspected

- `docs/adr/0022-use-hosted-linux-elf-execution.md`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/hosted_exec.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/syscall.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/process.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/signal.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/fault.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/hosted_exec.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tcti.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/pgtable.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tlbflush.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/mmu_context.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/block_cache.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tlb.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_user_page.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/Kconfig`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/Makefile`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/Makefile`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/Makefile`
- `OrlixKernel/Sources/ports/orlix/configs/release_defconfig`
- `OrlixKernel/Sources/ports/orlix/configs/development_defconfig`
- `OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk`
- `OrlixHostAdapter/Sources/OrlixHostAdapter/observability/log.c`
- `OrlixHostAdapter/Sources/OrlixHostAdapter/terminal/console.c`
- `OrlixHostAdapter/Sources/OrlixHostAdapter/boot/progress.c`
- `Makefile`
- `.agents/skills/orlix-tcti-next-step/SKILL.md`
- `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`
- `.agents/skills/orlix-tcti-next-step/scripts/status`
- `.agents/skills/orlix-tcti-next-step/scripts/next`
- `.agents/skills/orlix-tcti-next-step/scripts/task-envelope-check`
- `.agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift`
- `.codex/subagents/tcti-planner.md`
- `.codex/subagents/tcti-safety-reviewer.md`
- `.codex/subagents/tcti-oracle-engineer.md`
- `.codex/subagents/tcti-test-reducer.md`
- `.codex/subagents/tcti-release-gate-reviewer.md`
- `Build/AgentHarness/orlix-tcti/status.json`
- `Build/AgentHarness/orlix-tcti/next-task.json`
- `Build/AgentHarness/orlix-tcti/next-task.md`

## Current-State Audit

Record this before the next implementation patch changes behavior:

- `hosted_exec.c` already includes `<asm/tcti.h>`.
- `orlix_hosted_enter_user(struct pt_regs *regs)` already calls `orlix_tcti_enter_user(regs)` when `CONFIG_ORLIX_HOSTED_EXEC_TCTI` is enabled.
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tcti.h` already exists.
- `orlix_tcti_enter_user()` already exists in `hosted_exec/tcti/engine.c`.
- The current development KUnit/build audit passed:

```sh
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
  make -f OrlixKernel/Makefile kunit PROFILE=development
```

- Current hook state: compiled and linked in the development KUnit path, not stub-only, but runtime-incomplete and not physical-device proven.
- Pre-rails `development_defconfig` and `release_defconfig` set `CONFIG_ORLIX_HOSTED_EXEC_TCTI=y` and `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y`. That was a release-blocking inconsistency, not a runtime milestone. Product defconfigs must stay native-default until the gates below pass.

## Exact Files To Change

Architecture and plans:

- `docs/adr/0022-use-hosted-linux-elf-execution.md`
- `docs/investigations/orlix-tcti-reference-review.md`
- `docs/plans/active/orlix-tcti/PLAN.md`
- `docs/plans/active/orlix-tcti/IMPLEMENT.md`
- `docs/architecture/ORLIX_TCTI_48BIT_VA_ROADMAP.md`

Kernel config and build:

- `OrlixKernel/Sources/ports/orlix/configs/release_defconfig`
- `OrlixKernel/Sources/ports/orlix/configs/development_defconfig`
- `OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/Kconfig`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/Makefile`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/Makefile`

Kernel integration:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/hosted_exec.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/syscall.c`, only if a thin helper around the existing dispatch is required
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/process.c`, only for task/thread/TLS state integration
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/signal.c`, only if a shared user-exit helper is required
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/hosted_exec.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/processor.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/mmu_context.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/pgtable.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tlbflush.h`

TCTI files, existing or to extend:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tcti.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/mmu.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/Makefile`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/Makefile`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/block_cache.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/block_cache.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/gadget_program.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tlb.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tlb.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/report.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/report.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/gadgets/entry.S`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/gadgets/memory.S`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/gadgets/control.S`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/gadgets/arithmetic.S`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/gadgets/syscall.S`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/gadgets/tcti-gadget-gen.py`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/`

New MM integration:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_user_page.c`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_invalidate.c`

Validation tooling:

- `tools/runtime/orlix-runtime-validation.sh`
- `tools/orlix-a64-opprofile/`

## Build Configuration

Add Kconfig:

- `CONFIG_ORLIX_HOSTED_EXEC_NATIVE`
- `CONFIG_ORLIX_HOSTED_EXEC_TCTI`
- `CONFIG_ORLIX_TCTI_DEBUG_SWITCH`
- `CONFIG_ORLIX_TCTI_KUNIT_TEST`

Initial defaults:

- `CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y`
- `CONFIG_ORLIX_HOSTED_EXEC_TCTI=n`
- `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=n` in product defconfigs
- `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y` only when TCTI and debug/test configs enable it
- `CONFIG_ORLIX_TCTI_KUNIT_TEST=y` only in test/debug configs

`make tcti-gate TARGET=tcti-plan-consistency` must fail if product `development_defconfig` or `release_defconfig` re-enable `CONFIG_ORLIX_HOSTED_EXEC_TCTI=y` or `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y`, or if they lack `CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y`.

Reason for not flipping release default immediately:

- An unfinished TCTI scaffold must not replace the currently booting native path until `tcti-init-first-syscall` passes on physical iPhone. The ADR direction is TCTI as the first physical-iPhone beta backend, but the config flip is a gated implementation milestone.
- Test targets may enable TCTI through generated test configs. Product `development_defconfig` and `release_defconfig` must not default to TCTI until the gates below pass.

Development default may flip only after:

- `make tcti-gate TARGET=tcti-contract` passes
- `make tcti-gate TARGET=tcti-golden-elf` passes
- `make tcti-gate TARGET=tcti-diff-switch` passes
- `make tcti-gate TARGET=tcti-appstore-safety-audit` passes
- physical `tcti-init-first-syscall` passes

Release default may flip only after:

- all first gates pass
- release physical matrix passes
- JSON reports show no forbidden behavior

Build integration:

- Add `core-y += arch/orlix/hosted_exec/`.
- Add TCTI objects under `arch/orlix/hosted_exec/tcti/Makefile`.
- Add `tcti_user_page.o` and `tcti_invalidate.o` under `arch/orlix/mm/Makefile`.
- Add TCTI sources to `kbuild/kernel-rules.mk` so product archive builds include them, not only standalone Linux Kbuild.

## Hosted Exec Integration

Exact integration point:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/hosted_exec.c`
- Function: `void __noreturn orlix_hosted_enter_user(struct pt_regs *regs)`

Current shape to preserve and tighten:

```c
void __noreturn orlix_hosted_enter_user(struct pt_regs *regs)
{
	if (IS_ENABLED(CONFIG_ORLIX_HOSTED_EXEC_TCTI))
		orlix_tcti_enter_user(regs);

	if (!IS_ENABLED(CONFIG_ORLIX_HOSTED_EXEC_NATIVE))
		panic("Orlix: no hosted execution backend configured\n");

	orlix_hosted_resume_current_user(regs, true, 0);
}
```

This hook already exists. Future tasks must audit whether the existing hook is compiled, linked, and runtime-reachable in the selected config before proposing hosted-exec integration work.

Native backend handling:

- Keep native hosted execution as a dev/future backend.
- Do not delete current native trap-frame code in the first TCTI checkpoint.
- Do not use native host executable mappings for the physical-iPhone TCTI acceptance path.

TCTI entry loop:

- `orlix_tcti_enter_user(regs)` loops over `tcti_resume_user(current, task_pt_regs(current), current->mm)`.
- It handles only TCTI exit reasons and immediately returns control to Linux-owned syscall, fault, signal, yield, or task-exit paths.
- It must not create a second scheduler, process manager, syscall server, or VFS.

## Syscall Handoff

Exact syscall dispatch target:

- `long orlix_syscall_dispatch(struct pt_regs *regs)` in `arch/orlix/kernel/syscall.c`

Current helper semantics:

- `orlix_syscall_dispatch()` already sets the syscall return value.
- It already calls `orlix_timer_poll()`.
- It already calls `orlix_exit_to_user_mode_work(regs)`.
- It already calls `forget_syscall(regs)` when appropriate.

On guest `svc #0`:

1. TCTI exits with `TCTI_EXIT_SYSCALL`.
2. Set `regs->orig_x0 = regs->regs[0]`.
3. Set `regs->syscallno = regs->regs[8]`.
4. Advance `regs->pc += sizeof(u32)`.
5. Call `orlix_syscall_dispatch(regs)`.
6. Reload `regs = task_pt_regs(current)`.
7. Return to the TCTI loop without invoking exit-to-user work again.
8. Resume TCTI if the task still has `mm` and is not exiting.

If a later implementation needs separate raw syscall invocation, split `kernel/syscall.c` into:

- `orlix_syscall_invoke_raw(regs)`
- `orlix_syscall_dispatch(regs) = raw invoke + return value + timer + exit-to-user work + forget_syscall`

Until that split exists, TCTI must treat current `orlix_syscall_dispatch()` as the complete syscall path.

AArch64 Linux syscall ABI:

- `x8` is syscall number.
- `x0-x5` are syscall arguments.
- `x0` is return value.

HostAdapter must never see the syscall number.

Required test:

- `svc #0` enters `orlix_syscall_dispatch()` exactly once.
- `orlix_exit_to_user_mode_work(regs)` is not double-run by the TCTI caller.
- syscall return value is written to `x0`.
- `forget_syscall(regs)` has run when the helper requires it.

## TCTI Entry Contract

Core API:

```c
enum tcti_exit_reason {
	TCTI_EXIT_SYSCALL,
	TCTI_EXIT_USER_FAULT,
	TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
	TCTI_EXIT_SIGNAL_POINT,
	TCTI_EXIT_YIELD,
	TCTI_EXIT_TASK_EXIT,
};

struct tcti_result {
	enum tcti_exit_reason reason;
	long status;
	unsigned long fault_address;
	unsigned long pc;
	u32 instruction;
};

struct tcti_result tcti_resume_user(
	struct task_struct *task,
	struct pt_regs *regs,
	struct mm_struct *mm);
```

Contract:

- Execute the current Linux task userspace instructions until Linux needs control.
- Do not bypass Linux.
- Do not implement Linux syscall policy.
- Do not emulate Linux VFS/fd/signal/wait/process semantics.

## TCTI ABI Register And Calling Convention

C entry:

```c
int tcti_entry_block(
	struct tcti_gadget_word *program,
	struct tcti_cpu_state *cpu,
	struct tcti_tlb *tlb);
```

Register convention:

- host `x0`: gadget program pointer on entry, scratch after setup
- host `x1`: `struct tcti_cpu_state *`
- host `x2`: `struct tcti_tlb *`
- host `x28`: current gadget stream pointer
- host `x27`: next gadget pointer
- host `x26`: current guest PC cache or block metadata pointer, only if counters justify it
- host `x18/w18`: forbidden Apple platform register
- host `x16/x17`: scratch/IP only
- host `x19-x25`: hot guest register carriers only if entry/exit saves and restores Apple callee-saved ABI correctly
- host `x29`: host frame pointer preserved unless explicitly saved/restored
- host `x30`: host link register preserved across entry/exit

Guest state:

- Guest GPRs live in `tcti_cpu_state` backed by `pt_regs`.
- Hot guest registers may be cached in host registers according to selected mapping.
- 32-bit guest writes zero-extend into X registers.
- SP, XZR, and WZR behavior is explicit.
- NZCV/PSTATE is stored in `regs->pstate` or a dedicated `tcti_cpu_state->nzcv` field with deterministic commit-back.
- Guest TPIDR_EL0 lives in `current->thread.user_tls`.
- Host TPIDR_EL0 is never modified for guest behavior.

## Apple arm64 Host Register Rules

Host `x18/w18` is red:

- never guest `x18`
- never scratch
- never CPU-state pointer
- never TLB pointer
- never gadget-program pointer
- never inline-asm clobber
- never saved/restored as a workaround

Guest `x18` is ordinary virtual AArch64 state:

- it lives in guest CPU state, backed by `pt_regs` or a TCTI CPU-state wrapper
- it may be memory-backed
- it may be assigned to another safe host carrier when profiling proves it is hot
- it must never be assigned to host `x18/w18`

The static audit must parse assembly tokens, not comments. It must scan:

- handwritten TCTI `.S`
- generated TCTI `.S`, `.h`, and `.c` inline asm
- generator templates
- compiled disassembly for TCTI object files

Allow `x18/w18` only in comments explaining the ban and test fixtures explicitly named `x18_forbidden`.

## TPIDR_EL0 Transition Audit

Native hosted execution currently reads and writes host `TPIDR_EL0` in `hosted_exec.c`, including the native user TLS restore path and native syscall gate path. TCTI must not inherit those side effects accidentally.

Audit before changing TLS behavior:

- list every `mrs ..., tpidr_el0` and `msr tpidr_el0, ...` in `hosted_exec.c`
- mark each site native-hosted-only or shared
- define which sites are legal during TCTI entry and exit
- keep guest `TPIDR_EL0` in guest architectural state, currently `current->thread.user_tls`
- implement guest `MRS/MSR TPIDR_EL0` against guest state only
- never modify host `TPIDR_EL0` from TCTI gadgets

Required tests:

- guest `MSR TPIDR_EL0` changes guest TLS
- guest `MSR TPIDR_EL0` does not mutate host `TPIDR_EL0`
- guest `MRS TPIDR_EL0` reads the guest TLS value
- syscall, fault, and yield exits preserve guest TLS
- task switch preserves distinct guest TLS values

## Hot Register Mapping Candidates

Do not decide by intuition. Compare at least:

- Mapping A: fork-style guest `x0-x12` hot, `x13-x30` and SP memory-backed.
- Mapping B: syscall/call ABI hot set, `x0-x8`, `x19-x23`, cheap `x29-x30`, cheap SP.
- Mapping C: per-block register-carrier allocation based on decoded block register use.
- Mapping D: hybrid static hot set plus memory-backed cold registers.

Required counters:

- per-register read count
- per-register write count
- memory-backed register load/store count
- SP access count
- LR/x30 access count
- callee-saved register hotness
- syscall-boundary register hotness
- gadget count per instruction by mapping
- hot register spills/fills
- wall-clock impact by workload

## User Page Backing API

Explicit access classes:

```c
enum tcti_access {
	TCTI_ACCESS_FETCH,
	TCTI_ACCESS_READ,
	TCTI_ACCESS_WRITE,
};
```

User-page structure:

```c
struct tcti_user_page {
	unsigned long user_page;
	void *host_data;
	struct page *page;
	unsigned long linux_perms;
	u32 translation_generation;
	u32 code_generation;
	bool cow_sensitive;
	bool has_translated_blocks;
};
```

API:

```c
int tcti_pin_user_page(
	struct mm_struct *mm,
	unsigned long user_va,
	enum tcti_access access,
	struct tcti_user_page *out);

void tcti_unpin_user_page(struct tcti_user_page *page);
```

Rules:

- This lives in `arch/orlix/mm`, not HostAdapter.
- Linux MM is the authority.
- Do not assume `__va(PFN_PHYS(pte_pfn(entry)))` unless the Orlix hosted memory model proves that valid.
- `FETCH` requires Linux execute permission and reads bytes as host data. It does not imply guest data-read permission.
- `READ` requires Linux user read permission.
- `WRITE` requires Linux user write permission, handles CoW, and invalidates translated executable blocks when needed.
- Milestone 1 uses a safe pinned TLB with explicit page lifetime. No stale host pointers. No SIGSEGV recovery model.

## Generation Model

Add or extend `mm_context_t`:

```c
typedef struct {
	atomic_t tcti_translation_generation;
	atomic_t tcti_code_generation;
	struct tcti_mm_cache *tcti_cache;
} mm_context_t;
```

`translation_generation` invalidates TLB entries when user mappings or backing pointers change.

Increment `translation_generation` on:

- mmap
- munmap
- mprotect affecting access permissions
- CoW replacement
- page backing replacement
- exec
- address-space destruction

`code_generation` invalidates decoded blocks when executable mappings or executable bytes change.

Increment `code_generation` on:

- exec image replacement
- mmap of executable pages
- munmap of executable pages
- mprotect changing execute permission
- user PTE replacement when old or new mapping is user-executable
- CoW replacement affecting a translated executable page
- guest store to a page with translated blocks
- address-space destruction

Do not collapse these generations. One counter would over-invalidate hot decoded blocks during ordinary data churn.

## TCTI TLB

Structure:

```c
#define TCTI_TLB_BITS 13U
#define TCTI_TLB_SIZE (1U << TCTI_TLB_BITS)

struct tcti_tlb_entry {
	struct mm_struct *mm;
	unsigned long guest_page;
	void *host_page;
	struct page *page;
	u32 translation_generation;
	bool fetch_ok;
	bool read_ok;
	bool write_ok;
	bool cow_sensitive;
	bool has_translated_blocks;
};

struct tcti_tlb {
	struct mm_struct *active_mm;
	u32 active_generation;
	struct tcti_tlb_entry entries[TCTI_TLB_SIZE];
	u64 fetch_hits;
	u64 fetch_misses;
	u64 read_hits;
	u64 read_misses;
	u64 write_hits;
	u64 write_misses;
	u64 cross_page_reads;
	u64 cross_page_writes;
	u64 faults;
	u64 generation_flushes;
};
```

Index:

```c
((addr >> PAGE_SHIFT) ^ (addr >> (PAGE_SHIFT + TCTI_TLB_BITS))) &
	(TCTI_TLB_SIZE - 1)
```

Rules:

- Start with 8192 entries only if memory cost is acceptable, then tune from counters.
- Keep entries across syscalls only when `mm`, generation, access permissions, and pinned lifetime remain valid.
- Flush on mm switch, generation change, entry replacement, task exit, and exec.
- Ordinary `switch_mm` flushes the current thread TLB view only. It does not invalidate decoded blocks.

## Block Cache

Minimum cache key:

- `mm`
- `code_generation`
- guest start PC

Structure:

```c
#define TCTI_BLOCK_CACHE_BUCKETS 4096U
#define TCTI_PAGE_INDEX_BUCKETS 4096U
#define TCTI_DIRECT_PATCH_SLOTS 2U

struct tcti_gadget_word {
	unsigned long value;
};

struct tcti_patch_slot {
	struct tcti_block *source;
	unsigned long *slot;
	struct hlist_node target_link;
};

struct tcti_page_ref {
	unsigned long guest_page;
	struct hlist_node block_page_link;
	struct hlist_node page_index_link;
};

struct tcti_block {
	struct hlist_node hash_node;
	struct list_head mm_node;
	struct rcu_head rcu;
	refcount_t refs;
	struct mm_struct *mm;
	unsigned long guest_start_pc;
	unsigned long guest_end_pc;
	u32 code_generation;
	u32 instruction_count;
	u32 program_words;
	u32 page_count;
	unsigned long direct_target_pc[TCTI_DIRECT_PATCH_SLOTS];
	unsigned long *direct_jump_patch_slots[TCTI_DIRECT_PATCH_SLOTS];
	struct hlist_head incoming_patch_slots;
	struct tcti_page_ref *pages;
	struct tcti_gadget_word program[];
};

struct tcti_mm_cache {
	spinlock_t lock;
	atomic_t translation_generation;
	atomic_t code_generation;
	struct hlist_head block_hash[TCTI_BLOCK_CACHE_BUCKETS];
	struct hlist_head page_index[TCTI_PAGE_INDEX_BUCKETS];
	struct list_head blocks;
};
```

Cache levels:

- L0 per-thread direct cache keyed by PC and `code_generation`.
- L1 per-mm hash cache keyed by `mm`, `code_generation`, and guest start PC.

Required counters:

- blocks compiled
- L0 hits
- L1 hits
- misses
- hash average chain length
- hash max chain length
- invalidations
- page-index entries
- incoming slots unpatched
- deferred frees

## TCTI Concurrency Model

Milestone 1 chooses single TCTI runner per `mm`.

Compatibility warning: milestone 1 does not support multi-threaded userspace execution in one `mm`. Do not claim correctness for pthread workloads, dynamic language runtimes, package managers, shell job control, or fork/clone-heavy workloads until this restriction is lifted by the multi-runner rules below.

Rules:

- enforce with a per-mm lock or assertion before entering TCTI
- reject concurrent TCTI entry for the same `mm`
- document why this is acceptable for the first `/init` gate
- add a test proving concurrent entry is rejected
- L0 caches must not hold unpinned stale blocks across invalidation
- mm teardown must flush the runner state before address-space destruction completes

Multi-runner execution is a later milestone only. It requires:

- block lookup increments a ref
- the executing block is pinned
- invalidation unchains before retire
- L0 caches cannot hold unpinned stale blocks
- free happens only after a grace period
- mm teardown races are covered by tests

Do not implement half-RCU. Either milestone 1 is single-runner enforced, or the multi-runner rules above are fully implemented and tested.

## Invalidation Rules

Page-index reverse lookup is required before serious chaining.

Invalidate affected blocks and bump `code_generation` on:

- exec image replacement
- mmap of executable pages
- munmap of executable pages
- mprotect changing execute permission
- user PTE replacement when old or new mapping is user-executable
- CoW replacement affecting translated executable page
- guest store to a page with translated blocks
- address-space destruction

Bump `translation_generation` on:

- mmap
- munmap
- mprotect affecting access permissions
- CoW replacement
- page backing replacement
- exec
- address-space destruction

Guest stores to translated pages:

1. Fast path checks `page_has_translated_blocks`.
2. If false, store directly.
3. If true, slow path performs Linux write/CoW semantics.
4. Slow path invalidates translated blocks for that page.
5. Slow path bumps `code_generation` if executable code bytes changed.
6. Slow path bumps `translation_generation` if backing changed.
7. Slow path flushes affected TLB entries.
8. Store completes or retries according to the selected memory model.

Do not rely only on mprotect, munmap, or Linux flush hooks. Self-modifying code and writable executable mappings must be correct.

## Direct Block Chaining

Implement only after block cache and TLB correctness.

Milestone 1:

- same-page direct branch chaining only

Milestone 2:

- cross-page chaining only after source and target pages are page-indexed
- incoming slots are reverse-linked
- target invalidation unpatches all incoming slots
- tests cover unmap, mprotect, and write-to-code on source and target pages

Chaining patches only data slots in the gadget program. No executable code patching. No generated native code.

Every direct-chain implementation must prove:

- source block records outgoing patch slots
- target block records incoming patch slots
- page invalidation finds all overlapping blocks
- invalidation unpatches incoming slots before freeing targets
- a running block cannot jump into a freed target
- same-page chaining is tested before cross-page chaining

## Gadget Program And Generator

The gadget program is data:

```text
[gadget pointer, operand, operand, gadget pointer, operand, ...]
```

Execution model:

- entry gadget loads guest state pointer
- entry gadget loads TLB pointer
- entry gadget loads gadget-program pointer
- each gadget performs one guest instruction or fused pair
- each gadget advances the program pointer
- each gadget branches or tail-calls to the next gadget pointer

Generator rules:

- Source of truth is `tcti-gadget-gen.py`.
- Generated output is deterministic and disposable.
- Generated headers embed generator version/hash.
- Manual edits to generated output are invalid.

Assembly hot paths required early:

- entry/exit
- next-gadget dispatch
- load/store fast path
- direct branch
- conditional branch
- `SVC` exit
- `ADD/SUB` immediate/register
- `LDP/STP` stack-frame patterns
- `MOVZ/MOVK/MOVN`
- `MRS/MSR TPIDR_EL0`
- `MRS/MSR NZCV`

C gadgets are allowed for cold instructions during bring-up only.

## Debug Switch Backend

A naive switch interpreter is allowed only as a correctness oracle.

It must share:

- decoder
- instruction semantic helpers
- TLB slow path
- syscall handoff
- fault path
- unsupported-instruction reporting

Only dispatch differs.

It is not:

- the performance backend
- the beta backend
- the physical-iPhone default

## First Instruction Subset

Initial subset for ordinary static AArch64 Linux `/init`:

- `MOVZ/MOVK/MOVN`
- `ADR/ADRP`
- `ADD/SUB` immediate
- `ADD/SUB` shifted register
- `ADDS/SUBS`
- `CMP/CMN` aliases
- `AND/ORR/EOR` immediate/register
- scalar `LDR/STR` unsigned immediate for 8, 16, 32, and 64-bit widths
- `LDUR/STUR`
- `LDR` literal
- `LDRSB/LDRSH/LDRSW`
- `LDR/STR` register-offset when first trace requires it
- `LDP/STP` signed-offset, pre-index, and post-index for GPR pairs
- `B/BL/BR/BLR/RET`
- `CBZ/CBNZ`
- `TBZ/TBNZ`
- `B.cond`
- `NOP/HINT` as no-op or yield where appropriate
- `SVC #0`
- `MRS/MSR TPIDR_EL0`
- `MRS/MSR NZCV`

Do not start with:

- NEON
- crypto
- full atomics/exclusives
- Node
- Python
- Go
- Rust
- shell compatibility hacks
- OCI lifecycle work

## Unsupported Instruction Handling

Unsupported instruction must not hang silently.

Report:

- guest PC
- instruction word
- decoded class if known
- `x0-x30`
- SP
- PSTATE/NZCV
- VMA permissions
- task name and pid
- last syscall
- best-effort ELF segment/symbol

Then fail deterministically as Linux `SIGILL` or a controlled TCTI bring-up error, according to the current gate.

Every unsupported instruction that becomes supported must get a focused test.

## Trace-Led Opcode Expansion

Add `orlix-a64-opprofile`.

It must produce opcode/decode-class inventory for:

- static `/init`
- dynamic loader
- busybox/toybox
- apk
- OrlixMLibC test binaries
- coreutils sample binaries

Do not expand instruction support by guessing broadly.

## Autonomous Test Contract

No physical-device debugging is allowed until these pass:

- `make tcti-gate TARGET=tcti-contract`
- `make tcti-gate TARGET=tcti-toolchain-check`
- `make tcti-gate TARGET=tcti-golden-elf`
- `make tcti-gate TARGET=tcti-diff-switch`
- `make tcti-gate TARGET=tcti-memory-fuzz`
- `make tcti-gate TARGET=tcti-direct-chain-fuzz`
- `make tcti-gate TARGET=tcti-appstore-safety-audit`
- `make tcti-gate TARGET=tcti-report-schema-check`

Each target must:

- run locally without an iPhone
- avoid implementing a second Linux runtime
- produce machine-readable JSON under `Build/TCTI/reports/<target>/report.json`
- produce human-readable Markdown only as a secondary artifact under `Build/TCTI/reports/<target>/report.md`
- fail with a reducer artifact under `Build/TCTI/reproducers/<target>/<case-id>.json`
- print the exact next command to reproduce the failure, `make tcti-gate TARGET=tcti-repro REPRO=<path>`
- use the repository Swift rail driver and Swift/Foundation. Do not add Python for these rails.
- treat incomplete work as `status=todo`, `passed=false`, and exit non-zero

Allowed report statuses:

- `pass` means `passed=true`, release-gate eligible, readiness-gate eligible.
- `fail` means `passed=false`.
- `todo` means `passed=false` and exits non-zero.
- `skipped` means `passed=false` unless a specific gate explicitly allows it.
- `error` means `passed=false`.
- `evidence` means `passed=false`, `autonomous_tests_bypassed=true`, and is never release-gate or readiness-gate eligible.

Release/readiness gates only accept `status=pass`.

No TCTI implementation patch may begin by requiring the human to name a gate. The agent must first run:

- `make agent-status AREA=orlix-tcti`
- `make agent-next AREA=orlix-tcti`
- `make agent-task-envelope-check AREA=orlix-tcti`

The selected task envelope controls scope, forbidden work, validation commands, report paths, reducer requirements, subagents, and commit message for the next checkpoint.

Minimum report schema:

```json
{
  "target": "tcti-contract",
  "gate": "tcti-contract",
  "status": "todo",
  "passed": false,
  "summary": "short machine-readable summary",
  "git_sha": "",
  "backend": "tcti",
  "virtual_cpu_model": "orlix-aarch64-v1",
  "host_page_size": 16384,
  "guest_page_size": 4096,
  "forbidden_behavior": {
    "host_exec_guest_text": false,
    "map_jit": false,
    "rwx": false,
    "generated_exec_memory": false,
    "host_x18": false,
    "native_ios_api_exposure_to_guest": false
  },
  "counters": {},
  "failures": [],
  "artifacts": [],
  "release_gate_eligible": false,
  "readiness_gate_eligible": false,
  "autonomous_tests_bypassed": false,
  "bypass_reason": "",
  "coverage_warnings": []
}
```

`make tcti-gate TARGET=tcti-report-schema-check` validates:

- schema fixtures that must pass and must fail
- every `Build/TCTI/reports/**/report.json`
- top-level runtime JSON sidecars under `Build/Reports/runtime/*.json`

`make tcti-gate TARGET=tcti-contract` proves:

- decoder
- gadget ABI
- register commit-back
- SP/XZR/WZR
- NZCV/PSTATE
- TPIDR_EL0 guest state
- FETCH/READ/WRITE distinction
- TLB generation
- block-cache `code_generation`
- store-to-translated-page invalidation
- direct-chain patch/unpatch

`make tcti-gate TARGET=tcti-toolchain-check` proves the local no-phone toolchain can build static no-libc AArch64 Linux ELF artifacts and records:

- `clang` path and version
- `ld.lld` path and version
- `llvm-objdump` availability
- generated ELF type and header evidence
- source SHA256
- binary SHA256

If the local toolchain cannot build the seed ELF, this target must fail or emit `status=todo`. It must not pass.

`make tcti-gate TARGET=tcti-golden-elf` runs checked-in tiny static AArch64 Linux ELF artifacts:

- `init_001_exit`
- `init_002_write`
- `init_003_stack`
- `init_004_tls`
- `init_005_branches`
- `init_006_memory`
- `init_007_mprotect`
- `init_008_self_modify`
- `init_009_faults`
- `init_010_cpu_model`

Each corpus item has checked-in source and checked-in golden JSON under `OrlixKernel/Tests/TCTI/golden_elf/`, including:

- binary name
- generator command
- compiler path, compiler version, linker path, linker version
- compiler/linker flags
- libc or no-libc mode
- source SHA256
- expected binary SHA256
- actual binary SHA256 from the current run
- expected final registers
- expected memory changes
- expected syscalls
- expected signal or fault
- expected console output
- expected TCTI counters
- forbidden behavior expectations

Golden binaries are generated build artifacts unless a later checkpoint explicitly chooses to check them in. The source and golden JSON are canonical. If the generated binary hash differs from golden JSON, `make tcti-gate TARGET=tcti-golden-elf` fails and prints either the reducer command or `make tcti-gate TARGET=tcti-golden-elf-refresh CASE=<case-id>` after inspection.

`make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug` is the first no-phone execution proof. It may decode and execute only the seed `init_001_exit` MOVZ/SVC subset through the Swift rail's switch-debug harness and capture the guest `exit(42)` syscall event. It must not dispatch by exact full instruction word, host-execute guest code, call real host exit, call Darwin syscalls, implement Linux runtime semantics, use HostAdapter, run UIKit, run simulator gates, run physical-device gates, add production assembly, or implement gadget dispatch.

`make tcti-gate TARGET=tcti-golden-elf CASE=init_002_write EXECUTE=switch-debug` is the second no-phone execution proof. It may decode and execute only the emitted `init_002_write` subset: MOVZ 64-bit `hw=0`, ADR to an X register, and SVC `#0`. It may capture test-harness syscall events for `write(1, "hello\n", 6)` and `exit(0)`. The write capture may read bytes only from file-backed PT_LOAD guest memory. It must not write to host stdout, call Darwin `write`, implement fd tables, implement VFS, implement Linux process or signal semantics, use HostAdapter, run UIKit, run simulator gates, run physical-device gates, add production assembly, or implement gadget dispatch.

Physical-device gates must consume the same golden artifacts. They must not invent ad hoc device-only proof cases.

`make tcti-gate TARGET=tcti-diff-switch` compares the debug switch backend against the gadget backend:

- same decoder
- same memory model
- same syscall capture
- same fault capture
- different dispatch only

For every generated block, compare:

- GPRs
- SP
- PC
- PSTATE/NZCV
- TPIDR_EL0
- memory writes
- exit reason
- fault address

Do not implement an unrelated switch emulator. It would create a second bug surface.

Anti-drift rule: any semantic helper used by gadget lowering must be callable by the switch backend, or the differential test fails. Do not duplicate instruction semantics between `switch_debug.c` and generated gadgets.

`make tcti-gate TARGET=tcti-memory-fuzz` covers:

- simulated host page sizes: 4 KiB, 16 KiB, 64 KiB
- fixed Linux guest page size
- guest page backed by offset in a larger host page
- multiple guest pages inside one host allocation
- cross-page instruction fetch
- cross-page data read/write
- mprotect transitions
- munmap/remap
- CoW replacement
- store to translated executable page
- stale TLB generation
- stale `code_generation`

`make tcti-gate TARGET=tcti-direct-chain-fuzz` covers:

- source outgoing patch slots
- target incoming patch slots
- page-index overlap lookup
- invalidation unpatch before retire
- no jump into freed targets
- same-page chaining before cross-page chaining

`make tcti-gate TARGET=tcti-appstore-safety-audit` fails on:

- host `x18/w18` in TCTI assembly, generated gadgets, inline asm, or compiled TCTI object disassembly
- guest ELF text mapped host-executable
- `MAP_JIT` in the product path
- RWX mappings in the product path
- `vm_protect(... EXECUTE ...)` for guest text
- generated executable memory
- private executable-memory entitlements
- guest software exposed to native iOS APIs

The x18 scanner must tokenize source/disassembly lines, not comments-only grep. It scans handwritten `.S/.s`, C/H inline asm and templates, generated gadget outputs, generator templates, and compiled TCTI object disassembly. Docs may mention x18. Production TCTI comments should prefer "platform register" if the scanner cannot distinguish comments safely.

If no compiled TCTI objects exist yet, the report may pass with `coverage_warnings` and `scanned_objects=0`. If TCTI object files exist but disassembly cannot be produced, the audit fails.

`make tcti-gate TARGET=tcti-report-schema-check` validates every TCTI JSON report and runtime JSON sidecar and fails if a required field is absent, hand-written, or not parseable.

## Failure Reduction

Every failed gate must emit:

- failing binary or block id
- last successful guest PC
- faulting PC
- raw instruction word
- minimized block reproducer if possible
- exact host command to reproduce
- exact device command to rerun, when a device was involved
- trace artifact path

A physical-device failure may not be patched directly. It must first be reduced into one of:

- golden ELF test
- switch-vs-gadget differential block
- memory fuzz case
- direct-chain fuzz case
- appstore-safety audit case

No patching by reading iPhone logs and guessing.

## Runtime Validation Gates

First simulator gate command:

```sh
export PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
rtk proxy make runtime-validation \
  DESTINATION=iphonesimulator \
  GATE=tcti-init-first-syscall \
  ORLIX_SIMULATOR_ID=1E5553B0-203A-4A11-BAD7-EBDE46863F66 \
  ORLIX_TCTI_REQUIRED_SIMULATOR_ID=1E5553B0-203A-4A11-BAD7-EBDE46863F66 \
  ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max
```

Simulator readiness gates, in harness order:

- `tcti-init-first-syscall`
- `tcti-simulator-stability`
- `tcti-init-console-write`
- `tcti-static-busybox-start`
- `tcti-static-busybox-shell-command`
- `tcti-full-shell-usability`
- `tcti-package-behavior`
- `tcti-dynamic-loader-support`
- `tcti-signals`
- `tcti-vfs-completeness`
- `tcti-full-linux-runtime-readiness`

Every simulator report must be current for `HEAD`, passing, selected on the pinned simulator, single-booted, non-preflight, non-emergency-override, `backend=tcti`, `profile=tcti_runtime`, and free of fatal runtime artifacts. Marker gates must write their expected marker artifact, including shell usability, package behavior, dynamic loader support, signal behavior, VFS completeness, and full runtime readiness.

Phone work remains forbidden until `make agent-status AREA=orlix-tcti` reports `simulator_gates_complete=true` and `physical_device_allowed=true`, and until explicit human opt-in is present through `ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1` or `ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED=1`.

The physical target, when later eligible, must:

- refuse to run on a physical iPhone unless the Autonomous Test Contract targets pass
- refuse emergency evidence collection until the pinned simulator has current passing reports for first syscall, runtime stability, Linux console usability, static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader support, signals, VFS completeness, and full Linux runtime readiness
- build the requested profile
- auto-discover exactly one connected eligible physical iPhone through `xcrun devicectl --json-output`
- fail if zero or multiple devices match unless `ORLIX_DEVICE_ID` is supplied
- install and launch Orlix
- capture boot progress
- capture `linux-console` logs
- capture recent console mirror output
- capture `host-vm` trace lines
- capture TCTI counters
- write one JSON report under `REPORT_DIR`
- write one Markdown report under `REPORT_DIR`

Emergency override reports must use `status=evidence`, `passed=false`, `autonomous_tests_bypassed=true`, `release_gate_eligible=false`, and `readiness_gate_eligible=false`. An override can collect evidence. It cannot produce a passing gate.

TCTI gates:

- `tcti-init-first-syscall`
- `tcti-simulator-stability`, simulator-only, must fail if the first TCTI syscall marker is followed by fatal runtime evidence such as kernel panic, init death including both `Attempted to kill init` and `Attempted kill init`, user fault, BUG, Oops, SIGSEGV, fatal error, or crash.
- `tcti-init-console-write`
- `tcti-static-busybox-start`
- `tcti-dynamic-loader-start`
- `tcti-alpine-sh-start`

Later workload gates:

- `tcti-apk-basics`
- `tcti-python-node-smoke`
- `tcti-go-rust-smoke`
- `tcti-oci-mvp-command`

A gate does not pass if:

- it times out
- it is silently skipped
- it emits unexpected illegal instruction
- it emits unexpected page fault
- it emits unexpected `ENOSYS`
- it requires host executable mappings
- it requires modified guest binaries
- it bypasses Linux syscall dispatch
- its JSON report is missing or invalid
- `forbidden_behavior.host_exec_guest_text` is not false
- `forbidden_behavior.map_jit` is not false
- `forbidden_behavior.rwx` is not false
- `forbidden_behavior.generated_exec_memory` is not false
- `forbidden_behavior.native_ios_api_exposure_to_guest` is not false

## First Physical-Device Proof Target

Connected physical iPhone:

- ordinary static AArch64 Linux ELF `/init` is loaded by Linux `execve` and `binfmt_elf`
- `arch/orlix start_thread` initializes `pt_regs`
- `hosted_exec.c` selects TCTI
- TCTI enters the real ELF entrypoint
- TCTI reaches first real `svc #0`
- `svc #0` enters `orlix_syscall_dispatch`
- upstream Linux syscall implementation runs
- `/init` writes one line to Linux console
- output appears through Linux console, PTY, HostAdapter console mirror, and device logs
- `host-vm` logs prove no host executable mapping request for guest ELF text
- the JSON sidecar proves all forbidden-behavior fields are false

## Performance Counters

Every physical-device gate must report:

- guest instructions executed
- blocks compiled
- block cache L0 hits
- block cache L1 hits
- block cache misses
- block cache hit rate
- average guest instructions per block
- fetch TLB hits/misses
- read TLB hits/misses
- write TLB hits/misses
- TLB hit rate
- cross-page reads
- cross-page writes
- syscalls executed
- unsupported instructions
- guest faults
- `translation_generation` flushes
- `code_generation` invalidations
- direct-chain hits
- direct-chain misses
- exits per 1000 guest instructions
- hot register spills/fills
- memory-backed register loads/stores
- wall-clock time

No performance claim is valid without:

- exact binary workload
- device or simulator
- build configuration
- command
- baseline
- JSON report
- Markdown report

## Benchmark Ladder

Add `GATE=tcti-benchmark`.

Compare:

- `tcti_switch_debug`
- TCTI without block chaining
- TCTI with same-page block chaining
- TCTI with cross-page block chaining
- TCTI with fast TLB
- TCTI with assembly hot gadgets
- TCTI with mapping A/B/C/D hot-register strategy

Measure:

- blocks compiled
- cache hits/misses
- guest instructions
- average instructions per block
- TLB hits/misses
- direct-chain hits
- syscalls
- unsupported instructions
- guest faults
- hot register spill/fill count
- memory-backed register accesses
- wall-clock time

Do not implement speculative hot traces until a benchmark proves it improves a real workload enough to justify complexity.

## 48-Bit VA And Future Runtime Compatibility

Add a design note covering:

- 48-bit guest virtual address strategy
- high mmap hint handling
- `MAP_NORESERVE` lazy reservation behavior
- low guard-page behavior
- large VA reservations for V8, Go, Rust, and JVM
- futex roadmap
- `sigaltstack` roadmap
- AArch64 signal `ucontext` roadmap
- atomics/exclusives roadmap
- NEON/crypto roadmap

This is a design note, not first implementation scope.

## Host Capability And Virtual CPU Model

Do not expose physical iPhone CPU variability directly to Linux userspace.

Initial guest-visible CPU model:

- `orlix-aarch64-v1`
- baseline AArch64 integer
- guest `TPIDR_EL0`
- guest NZCV/PSTATE
- no LSE until atomics/exclusives are correct
- no crypto until crypto gadgets are correct
- no SVE
- no MTE
- no guest PAC/BTI until explicitly implemented
- baseline FP/SIMD only after implemented and tested

Guest-visible capability surfaces must derive from `orlix-aarch64-v1`, not the physical host:

- `AT_HWCAP`
- `AT_HWCAP2`
- `CTR_EL0`
- `DCZID_EL0`
- ID registers, if exposed
- `/proc/cpuinfo`

Host CPU features may choose faster internal gadget implementations only when guest-visible semantics do not change.

Required proof:

- `init_010_cpu_model` in the golden ELF corpus reads auxv, cpuinfo, and supported sysreg surfaces, then verifies they match `orlix-aarch64-v1`.
- JSON reports include `virtual_cpu_model`.

Host capability report at app startup:

```json
{
  "device_model": "",
  "chip_family": "",
  "ios_version": "",
  "host_page_size": 0,
  "guest_page_size": 4096,
  "jit_available": false,
  "map_jit_available": false,
  "guest_text_host_executable": false,
  "tcti_backend": "safe-nojit",
  "virtual_cpu_model": "orlix-aarch64-v1"
}
```

TCTI may depend on this report only through a tiny capability layer. It must not let host capability variation leak into Linux userspace.

## Virtio Boundary

Do not use virtio to define the virtual CPU model.

`arch/orlix` plus TCTI owns:

- guest AArch64 instruction semantics
- guest register state
- guest system-register behavior
- `AT_HWCAP` / `AT_HWCAP2` / cpuinfo policy
- syscall, fault, and yield exits

Virtio may expose Linux-visible devices:

- block/rootfs
- console
- rng
- network
- filesystem sharing
- host service transport
- future input/GPU devices

Virtio must not expose:

- instruction execution
- native iOS API calls
- syscall dispatch
- process creation
- arbitrary host filesystem access
- CPU feature probing

No virtio device may become a syscall escape hatch or HostAdapter ABI backdoor.

## Device Matrix

Physical devices certify the already-proven host contract. They are not where instruction semantics are first debugged.

Tier 0, every commit:

- autonomous TCTI targets
- appstore-safety audit
- report-schema check

Tier 1, every PR or nightly:

- Orlix integration tests
- simulator smoke only for app wiring

Tier 2, nightly physical smoke:

- one physical iPhone
- current iOS
- `tcti-init-first-syscall`
- `tcti-init-console-write`
- no host executable guest text

Tier 3, release physical matrix:

- oldest supported chip
- current mainstream chip
- newest chip
- iPad/M-series if supported
- oldest supported iOS version
- current stable iOS
- latest beta as advisory only

Tier 4, pre-TestFlight soak:

- shell/coreutils workload
- memory pressure
- background/foreground lifecycle
- repeated launch
- thermal throttling

When a device fails, collect a structured trace, reduce it into the autonomous harness, fix the contract-layer bug, rerun host/oracle tests, then rerun the physical gate.

## App Store Product Constraint

Treat App Store compatibility as an executable engineering constraint.

Phase 1 product shape:

- bundled curated Linux environment
- bundled tools
- user source editing and compilation where product policy permits
- no arbitrary package repository UI
- no guest executable host mappings
- no native iOS APIs exposed to guest software

Later phases require explicit App Review/legal strategy before:

- user-owned rootfs import
- arbitrary package-manager downloads
- package repositories that introduce new executable functionality after review

The audit target is the enforcement mechanism. Do not claim App Store safety from intent or guideline citations alone.

## Tests

Ownership split:

- KUnit owns kernel-bound unit contracts inside `arch/orlix`: syscall handoff, fault boundaries, mm interaction, invalidation hooks, task/thread state, and TCTI ABI invariants.
- Swift/host tools own golden ELF build metadata, report schema validation, reducer replay, appstore-safety scanning, differential block execution, memory fuzzing, direct-chain fuzzing, agent status, next-task selection, task-envelope validation, and no-phone gate orchestration.
- XCTest/runtime validation owns app packaging, HostAdapter mediation, simulator wiring, physical-device lifecycle, device logs, and no-forbidden-host-behavior runtime evidence.

Agent harness checks:

- `make agent-status AREA=orlix-tcti` writes status JSON and prints pass, fail, todo, and missing state per roadmap gate.
- `make agent-next AREA=orlix-tcti` writes next-task JSON and Markdown.
- `make agent-task-envelope-check AREA=orlix-tcti` validates roadmap membership, prerequisites, forbidden scope, validation commands, expected report paths, no custom MCP, no `tools/agent`, and physical/gadget blocking rules.

KUnit tests under the TCTI area:

- TCTI ABI invariants
- register state
- hot-register mapping correctness
- memory-backed register correctness
- 32-bit register write zero-extension
- SP/XZR/WZR behavior
- NZCV/PSTATE behavior
- decode coverage for first subset
- branch/call/return
- load/store
- cross-page access
- FETCH vs READ vs WRITE permission distinction
- Linux execute-permission fault
- Linux page-fault propagation
- `svc #0` enters existing `orlix_syscall_dispatch`
- `svc #0` does not double-run exit-to-user work
- TPIDR_EL0 TLS behavior
- guest `MSR TPIDR_EL0` does not mutate host `TPIDR_EL0`
- unsupported-instruction reporting
- block-cache lookup
- page-index invalidation
- same-page direct-chain patching
- direct-chain unpatching
- TLB hit/miss/generation flush
- self-modifying executable-page invalidation
- guest store to translated page invalidation
- single-runner-per-mm enforcement
- virtual CPU model surfaces
- x18 forbidden-token audit fixtures
- JSON report schema validation
- golden ELF corpus execution
- switch-vs-gadget differential execution
- memory fuzz with simulated host page sizes
- direct-chain fuzz

HostAdapter/XCTest coverage only for:

- TCTI gate does not call host executable mapping for guest ELF text
- console mirror captures guest output
- host capability report emits no forbidden executable-memory behavior

Do not add raw string diagnostics or make UIKit the proof surface.

## Reference Review

Primary reference:

- `https://github.com/rudironsoni/ish`
- branch `feat/aarch64-migration`
- commit reviewed: `55d14a9fefe47a7ed9b3bb44e4ccca7429bd9363`

Primary files read:

- `README.md`
- `project.yml`
- `docs/plans/a64-tcti-proof-program.md`
- `Sources/IXLandLinuxRuntime/emu/aarch64/cpu.c`
- `Sources/IXLandLinuxRuntime/emu/aarch64/cpu.h`
- `Sources/IXLandLinuxRuntime/emu/aarch64/block-cache.h`
- `Sources/IXLandLinuxRuntime/emu/aarch64/block-cache.c`
- `Sources/IXLandLinuxRuntime/emu/aarch64/fetch.h`
- `Sources/IXLandLinuxRuntime/emu/aarch64/fetch.c`
- `Sources/IXLandLinuxRuntime/emu/aarch64/memory.c`
- `Sources/IXLandLinuxRuntime/emu/aarch64/sysreg.c`
- `Sources/IXLandLinuxRuntime/emu/mmu.h`
- `Sources/IXLandLinuxRuntime/emu/tlb.h`
- `Sources/IXLandLinuxRuntime/emu/tlb.c`
- `Sources/IXLandLinuxRuntime/tcti/frame.h`
- `Sources/IXLandLinuxRuntime/tcti/aarch64/gen.h`
- `Sources/IXLandLinuxRuntime/tcti/aarch64/gen.c`
- `Sources/IXLandLinuxRuntime/tcti/aarch64/tcti-gadget-gen.py`
- `Sources/IXLandLinuxRuntime/kernel/memory.c`
- `Sources/IXLandLinuxRuntime/kernel/page_map.c`

Concepts carried:

- AArch64 guest-only TCTI path
- generated gadget table from source-of-truth generator
- data-only gadget stream
- `tcti_entry_block(gadgets, cpu_state)` entry shape
- generation-stamped TLB
- separate translation generation and code generation
- host `PROT_READ` for guest executable pages, never host `PROT_EXEC`
- fetch/decode/lowering/dispatch/exit instrumentation vocabulary
- contract/system/perf/UI test separation

Concepts not copied blindly:

- IXLand runtime ownership model
- iSH process model
- iSH fakefs
- iSH syscall emulator
- native offload
- bind-mount shortcuts
- DebugServer/app agent API
- x0-x12-only hot register mapping as final design
- hardcoded ldso PC-range diagnostics
- O(n) whole-cache invalidation
- no-lock block cache
- fixed 1024-entry TLB
- READ/FETCH permission conflation
- SIGSEGV-based recovery as milestone-1 memory model

Migration accountability:

| Fork feature | Orlix equivalent | Copied? | Proof |
| --- | --- | --- | --- |
| generation-stamped TLB | `arch/orlix` TCTI TLB | no | `make tcti-gate TARGET=tcti-memory-fuzz` generation tests |
| separate translation/code generations | `tcti_mm_cache` generations | no | self-modifying executable-page invalidation test |
| data-only gadget stream | Orlix gadget program | no | `make tcti-gate TARGET=tcti-diff-switch` |
| `tcti_entry_block(gadgets, cpu)` shape | `tcti_entry_block(program, cpu, tlb)` | no | gadget ABI contract test |
| generated gadget tables | Orlix generator | no | deterministic generator audit |
| `x0-x12` hot mapping | benchmark candidate A only | no | `make tcti-gate TARGET=tcti-benchmark` mapping comparison |
| READ/FETCH conflation | split FETCH/READ/WRITE | no | permission contract tests |
| O(n) invalidation | page-index invalidation | no | `make tcti-gate TARGET=tcti-direct-chain-fuzz` and invalidation tests |
| SIGSEGV recovery | deferred beyond milestone 1 | no | safe pinned TLB tests |

Secondary references read for comparison:

- `https://github.com/ish-app/ish`
- `https://github.com/OpenMinis/ish-arm64`
- `https://github.com/OpenMinis/ish-arm64/blob/master/README_arm64.md`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/asbestos.c`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/asbestos.h`
- `https://github.com/OpenMinis/ish-arm64/blob/master/asbestos/guest-arm64/gen.c`
- `https://github.com/OpenMinis/ish-arm64/tree/master/asbestos/guest-arm64/gadgets-aarch64`
- `https://github.com/OpenMinis/ish-arm64/blob/master/emu/tlb.h`
- `https://github.com/OpenMinis/ish-arm64/blob/master/kernel/arch/arm64/calls.c`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/ARM64_BACKEND.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/RUNTIME_VALIDATION.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/ARM64_WORKLOAD_SMOKE_TESTS.md`
- `https://github.com/rcarmo/ios-linuxkit/blob/master/docs/LINUX_BUILD_AND_HOST_ABI.md`

Current public references to preserve:

- Apple App Review Guidelines: `https://developer.apple.com/app-store/review/guidelines/`
- Apple arm64 `x18` guidance: `https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms`
- QEMU TCG direct chaining and invalidation: `https://www.qemu.org/docs/master/devel/tcg.html`
- Virtio 1.3 specification: `https://docs.oasis-open.org/virtio/virtio/v1.3/virtio-v1.3.html`
- UTM iOS no-JIT precedent: `https://docs.getutm.app/installation/ios/`

## License And Source-Copy Rules

No iSH, OpenMinis, or ios-linuxkit source is copied in the first implementation.

Any future copied source requires:

- source repository
- source file
- source commit
- license
- attribution requirement
- GPL compatibility with OrlixKernel/Linux distribution path
- App Store distribution implications
- reason clean-room implementation is insufficient

## Implementation Order

1. Record current-state audit: existing TCTI hook, `asm/tcti.h`, `orlix_tcti_enter_user()`, defconfig state, and development build result.
2. Correct build defaults so product profiles do not silently default to an unproved TCTI backend.
3. Fix syscall handoff semantics so current `orlix_syscall_dispatch()` owns exit-to-user work for this path.
4. Add autonomous test targets and JSON/reducer report contracts before physical-device work.
5. Add the agent-neutral autonomous next-task loop:
   - roadmap: `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`
   - status: `make agent-status AREA=orlix-tcti`
   - selection: `make agent-next AREA=orlix-tcti`
   - validation: `make agent-task-envelope-check AREA=orlix-tcti`
6. For every future checkpoint, implement only the selected envelope scope. Do not jump to assembly, gadgets, simulator, or device work unless envelope prerequisites allow it.
7. Continue the harness-selected gate. The current selected gate is `simulator-tcti-runtime-stability`, because no-phone oracle and simulator first-syscall prerequisites are already proven and the latest simulator stability report still fails after `clone(SIGCHLD)` with a TCTI fetch at `pc=0`.
8. Add TPIDR_EL0 transition audit and tests separating guest TLS from host TLS.
9. Add host `x18/w18` static and object-disassembly audit.
10. Add single-runner-per-mm enforcement for milestone 1.
11. Add safe `FETCH/READ/WRITE` user-page backing API and pinned TLB lifetime.
12. Add decoder and first instruction semantic helpers only from selected golden ELF envelopes.
13. Add block cache with `code_generation` and page-index invalidation.
14. Add TLB with `translation_generation` and host-page-size fuzzing.
15. Add assembly entry/dispatch and first hot gadgets only after switch-debug oracle coverage and differential prerequisites pass.
16. Add same-page direct chaining and `make tcti-gate TARGET=tcti-direct-chain-fuzz`.
17. Add hot-register mapping counters and compare mappings A/B/C/D.
18. Add runtime-validation gate with JSON and Markdown reports.
19. Expand instructions only from `orlix-a64-opprofile` traces and focused tests.
20. Add cross-page chaining, then atomics/exclusives, then NEON/SIMD, then crypto only after benchmark evidence justifies each expansion.

## Current Checkpoint Scope

The current harness checkpoint is simulator-first and reducer-backed:

- no-phone golden ELF and switch-debug oracle prerequisites have advanced past the early seed gates
- simulator first-syscall evidence exists for the pinned `Orlix-iPhone-15-Pro-Max` simulator
- physical-device work remains blocked while any required pinned simulator ladder report is missing or failing
- the current simulator failure is reduced as clone/post-syscall return-frame preservation, not as an unreduced phone log
- current scoped production edits are allowed only when they are tied to the reducer-backed simulator stability envelope
- current next gate selected as `simulator-tcti-runtime-stability`
- forbidden scope still blocks physical-device gates, production assembly, broad gadget expansion, HostAdapter Linux behavior, Darwin syscall behavior, Linux runtime semantics outside upstream Linux ownership, generated-tree edits, product defconfig flips, host-executable guest text, MAP_JIT, RWX, and generated executable memory

This checkpoint is not release-ready. It is not a performance claim. It is not a physical-device proof.
It is not the full TCTI goal. It must remain open until the completion claim boundary above is satisfied by reports, simulator stability, physical-device evidence, and release/readiness eligibility.

## Final Architecture Sentence

Orlix TCTI is an Orlix-owned, arch/orlix, no-JIT, same-ISA, tail-call-threaded user-instruction backend for unmodified AArch64 Linux ELF binaries. It does not replace Linux; it lets OrlixKernel’s existing Linux userspace surface run on iOS without host-executable guest text.
