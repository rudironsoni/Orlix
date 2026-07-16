/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ENGINE_H
#define ORLIX_TCTI_ENGINE_H

#include <linux/elf.h>
#include <asm/tcti.h>

void tcti_prepare_syscall_handoff(struct pt_regs *regs);
bool tcti_prepare_successful_execve_return(struct pt_regs *regs);
bool tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls);
#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
bool tcti_syscall_changes_user_mappings_for_tests(unsigned long nr);
#endif
#endif /* ORLIX_TCTI_ENGINE_H */
