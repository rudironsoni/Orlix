/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SWITCH_DEBUG_H
#define ORLIX_TCTI_SWITCH_DEBUG_H

#include <asm/orlix_tcti.h>

struct orlix_tcti_decoded_instruction;

int orlix_tcti_switch_debug_execute_decoded(struct mm_struct *mm,
				      struct pt_regs *regs,
				      const struct orlix_tcti_decoded_instruction *decoded,
				      unsigned long *fault_address);
struct orlix_tcti_result orlix_tcti_switch_debug_resume_user(struct task_struct *task,
							 struct pt_regs *regs,
							 struct mm_struct *mm);

#ifdef CONFIG_ORLIX_TCTI_KUNIT_TEST
/* Samples the N-form release publication from its production execution site. */
void orlix_tcti_memory_set_set_ordering_test_hook(
	void (*hook)(void *data, const struct pt_regs *regs, u8 destination_reg,
		     unsigned long destination), void *data);
#endif

#endif /* ORLIX_TCTI_SWITCH_DEBUG_H */
