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

void tcti_prepare_syscall_handoff(struct pt_regs *regs);
bool tcti_prepare_successful_execve_return(struct pt_regs *regs);
bool tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls);
bool tcti_kernel_syscall_dispatch_smoke_for_tests(
	struct pt_regs *regs,
	struct tcti_kernel_syscall_dispatch_smoke_result *out);

#endif /* ORLIX_TCTI_ENGINE_H */
