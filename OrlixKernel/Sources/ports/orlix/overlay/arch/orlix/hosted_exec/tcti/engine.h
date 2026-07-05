/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ENGINE_H
#define ORLIX_TCTI_ENGINE_H

#include <linux/elf.h>
#include <asm/tcti.h>

struct tcti_kernel_syscall_dispatch_smoke_result {
	bool decoded_svc;
	bool svc_boundary_reached;
	bool handoff_prepared;
	unsigned long observed_syscall_nr;
	bool orlix_syscall_dispatch_entered;
	bool linux_return_state_written;
	long return_value;
	unsigned long return_x0;
	s32 syscallno_after_dispatch;
};

struct tcti_kernel_execve_binfmt_elf_smoke_payload {
	unsigned char elf_class;
	unsigned char elf_data;
	unsigned short elf_type;
	unsigned short elf_machine;
	unsigned int load_segment_count;
	unsigned long entry_pc;
	unsigned long stack_top;
};

struct tcti_kernel_execve_binfmt_elf_smoke_result {
	bool payload_is_elf64;
	bool payload_is_little_endian;
	bool payload_is_aarch64;
	bool payload_has_load_segment;
	bool payload_type_supported;
	bool arch_accepts_payload;
	bool start_thread_called;
	bool entry_pc_recorded;
	bool stack_pointer_recorded;
	bool user_mode_prepared;
	bool syscall_state_cleared;
	unsigned long entry_pc;
	unsigned long stack_pointer;
	s32 syscallno_after_start_thread;
};

void tcti_prepare_syscall_handoff(struct pt_regs *regs);
bool tcti_prepare_successful_execve_return(struct pt_regs *regs);
bool tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls);
bool tcti_kernel_syscall_dispatch_smoke_for_tests(
	struct pt_regs *regs,
	struct tcti_kernel_syscall_dispatch_smoke_result *out);
bool tcti_kernel_execve_binfmt_elf_smoke_for_tests(
	const struct tcti_kernel_execve_binfmt_elf_smoke_payload *payload,
	struct pt_regs *regs,
	struct tcti_kernel_execve_binfmt_elf_smoke_result *out);

#endif /* ORLIX_TCTI_ENGINE_H */
