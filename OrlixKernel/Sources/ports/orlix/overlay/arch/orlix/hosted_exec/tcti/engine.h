/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ENGINE_H
#define ORLIX_TCTI_ENGINE_H

#include <linux/elf.h>
#include <asm/tcti.h>

void tcti_prepare_syscall_handoff(struct pt_regs *regs);
bool tcti_prepare_successful_execve_return(struct pt_regs *regs);
bool tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls);

#endif /* ORLIX_TCTI_ENGINE_H */
