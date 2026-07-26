/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ENGINE_H
#define ORLIX_TCTI_ENGINE_H

#include <linux/elf.h>
#include <asm/orlix_tcti.h>

void orlix_tcti_prepare_syscall_handoff(struct pt_regs *regs);
bool orlix_tcti_prepare_successful_execve_return(struct pt_regs *regs);
bool orlix_tcti_static_pie_relocation_count_valid(size_t count);
bool orlix_tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls);
#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
bool orlix_tcti_syscall_changes_user_mappings_for_tests(unsigned long nr);
void orlix_tcti_invalidate_changed_user_mappings_for_tests(struct mm_struct *mm,
						      unsigned long nr,
						      long status);
#endif
#endif /* ORLIX_TCTI_ENGINE_H */
