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
- Physical iPhone gate proves static `/init` reaches real `svc #0`, enters `orlix_syscall_dispatch`, writes one Linux console line, and emits HostAdapter console mirror evidence.
- Performance claims include exact workload, device, build configuration, command, baseline, counters, wall-clock result, and Markdown report.

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
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/pgtable.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tlbflush.h`
- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/mmu_context.h`
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

New TCTI files:

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
- `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y` only when TCTI and debug/test configs enable it
- `CONFIG_ORLIX_TCTI_KUNIT_TEST=y` only in test/debug configs

Reason for not flipping release default immediately:

- An unfinished TCTI scaffold must not replace the currently booting native path until `tcti-init-first-syscall` passes on physical iPhone. The ADR direction is TCTI as the first physical-iPhone beta backend, but the config flip is a gated implementation milestone.

Build integration:

- Add `core-y += arch/orlix/hosted_exec/`.
- Add TCTI objects under `arch/orlix/hosted_exec/tcti/Makefile`.
- Add `tcti_user_page.o` and `tcti_invalidate.o` under `arch/orlix/mm/Makefile`.
- Add TCTI sources to `kbuild/kernel-rules.mk` so product archive builds include them, not only standalone Linux Kbuild.

## Hosted Exec Integration

Exact integration point:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/hosted_exec.c`
- Function: `void __noreturn orlix_hosted_enter_user(struct pt_regs *regs)`

Required shape:

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

On guest `svc #0`:

1. TCTI exits with `TCTI_EXIT_SYSCALL`.
2. Set `regs->orig_x0 = regs->regs[0]`.
3. Set `regs->syscallno = regs->regs[8]`.
4. Advance `regs->pc += sizeof(u32)`.
5. Call `orlix_syscall_dispatch(regs)`.
6. Reload `regs = task_pt_regs(current)`.
7. Run the existing arch/orlix exit-to-user signal/task-work/schedule path.
8. Resume TCTI if the task still has `mm` and is not exiting.

AArch64 Linux syscall ABI:

- `x8` is syscall number.
- `x0-x5` are syscall arguments.
- `x0` is return value.

HostAdapter must never see the syscall number.

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
- host `x18`: reserved, never guest state on Apple arm64
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

## Runtime Validation Gates

First gate command:

```sh
export PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
rtk proxy make runtime-validation \
  PROFILE=development \
  GATE=tcti-init-first-syscall \
  DESTINATION=iphoneos \
  REPORT_DIR="$(external-ssd-root)/Xcode/OrlixSystem/Reports/runtime"
```

The target must:

- build the requested profile
- auto-discover exactly one connected eligible physical iPhone through `xcrun devicectl --json-output`
- fail if zero or multiple devices match unless `ORLIX_DEVICE_ID` is supplied
- install and launch Orlix
- capture boot progress
- capture `linux-console` logs
- capture recent console mirror output
- capture `host-vm` trace lines
- capture TCTI counters
- write one Markdown report under `REPORT_DIR`

TCTI gates:

- `tcti-init-first-syscall`
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

## Tests

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
- TPIDR_EL0 TLS behavior
- unsupported-instruction reporting
- block-cache lookup
- page-index invalidation
- same-page direct-chain patching
- direct-chain unpatching
- TLB hit/miss/generation flush
- self-modifying executable-page invalidation
- guest store to translated page invalidation

HostAdapter/XCTest coverage only for:

- TCTI gate does not call host executable mapping for guest ELF text
- console mirror captures guest output

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

1. Update ADR 0022 and active plan with physical-device evidence and TCTI correction.
2. Add TCTI config/build scaffolding without flipping release default before proof.
3. Add TCTI ABI and debug switch oracle.
4. Add safe `FETCH/READ/WRITE` user-page backing API.
5. Add decoder and first instruction semantic helpers.
6. Add block cache with `code_generation`.
7. Add TLB with `translation_generation`.
8. Wire `hosted_exec.c` to call TCTI and call `orlix_syscall_dispatch` on `svc #0`.
9. Add assembly entry/dispatch and first hot gadgets.
10. Add page-index invalidation and guest store-to-translated-page invalidation.
11. Add same-page direct chaining.
12. Add hot-register mapping counters and compare mappings A/B/C/D.
13. Add runtime-validation gate and physical-device Markdown report.
14. Expand instructions only from `orlix-a64-opprofile` traces and focused tests.
15. Add cross-page chaining, then atomics/exclusives, then NEON/SIMD, then crypto only after benchmark evidence justifies each expansion.

## Current Checkpoint Scope

The first implementation checkpoint is intentionally narrow:

- active plan and reference review
- ADR 0022 correction
- Kconfig/build source visibility
- TCTI public arch interface
- debug switch backend stub
- block cache and TLB structure headers
- user-page API stub
- hosted-exec selection hook

This checkpoint is not runtime-ready. It is not a performance claim. It is not a physical-device proof.

## Final Architecture Sentence

Orlix TCTI is an Orlix-owned, arch/orlix, no-JIT, same-ISA, tail-call-threaded user-instruction backend for unmodified AArch64 Linux ELF binaries. It does not replace Linux; it lets OrlixKernel's existing Linux userspace surface run on iOS without host-executable guest text.
