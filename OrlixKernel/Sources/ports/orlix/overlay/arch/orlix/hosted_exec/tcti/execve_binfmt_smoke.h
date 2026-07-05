/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_EXECVE_BINFMT_SMOKE_H
#define ORLIX_TCTI_EXECVE_BINFMT_SMOKE_H

#ifndef ELFCLASS64
#define ELFCLASS64 2
#endif

#ifndef ELFDATA2LSB
#define ELFDATA2LSB 1
#endif

#ifndef ET_EXEC
#define ET_EXEC 2
#endif

#ifndef ET_DYN
#define ET_DYN 3
#endif

#ifndef EM_AARCH64
#define EM_AARCH64 183
#endif

#ifndef PSR_MODE_MASK
#define PSR_MODE_MASK 0xfUL
#endif

#ifndef PSR_MODE_EL0t
#define PSR_MODE_EL0t 0x00000000UL
#endif

typedef void (*tcti_execve_binfmt_start_thread_fn)(struct pt_regs *regs,
						   unsigned long pc,
						   unsigned long sp);

static inline bool tcti_execve_binfmt_user_mode(const struct pt_regs *regs)
{
	return (regs->pstate & PSR_MODE_MASK) == PSR_MODE_EL0t;
}

static inline bool tcti_kernel_execve_binfmt_elf_smoke_execute(
	const struct tcti_kernel_execve_binfmt_elf_smoke_payload *payload,
	struct pt_regs *regs,
	struct tcti_kernel_execve_binfmt_elf_smoke_result *out,
	tcti_execve_binfmt_start_thread_fn start_thread)
{
	unsigned long sp;

	if (!payload || !regs || !out || !start_thread)
		return false;

	memset(out, 0, sizeof(*out));
	out->payload_is_elf64 = payload->elf_class == ELFCLASS64;
	out->payload_is_little_endian = payload->elf_data == ELFDATA2LSB;
	out->payload_is_aarch64 = payload->elf_machine == EM_AARCH64;
	out->payload_has_load_segment = payload->load_segment_count > 0;
	out->payload_type_supported = payload->elf_type == ET_EXEC ||
				      payload->elf_type == ET_DYN;
	out->arch_accepts_payload = out->payload_is_elf64 &&
				    out->payload_is_little_endian &&
				    out->payload_is_aarch64 &&
				    out->payload_has_load_segment &&
				    out->payload_type_supported &&
				    payload->entry_pc != 0 &&
				    payload->stack_top > sizeof(unsigned long);
	if (!out->arch_accepts_payload)
		return false;

	sp = payload->stack_top - sizeof(unsigned long);
	start_thread(regs, payload->entry_pc, sp);
	out->start_thread_called = true;
	out->entry_pc = regs->pc;
	out->stack_pointer = regs->sp;
	out->syscallno_after_start_thread = regs->syscallno;
	out->entry_pc_recorded = regs->pc == payload->entry_pc;
	out->stack_pointer_recorded = regs->sp == sp;
	out->user_mode_prepared = tcti_execve_binfmt_user_mode(regs);
	out->syscall_state_cleared = regs->syscallno == NO_SYSCALL;

	return out->entry_pc_recorded &&
	       out->stack_pointer_recorded &&
	       out->user_mode_prepared &&
	       out->syscall_state_cleared;
}

#endif /* ORLIX_TCTI_EXECVE_BINFMT_SMOKE_H */
