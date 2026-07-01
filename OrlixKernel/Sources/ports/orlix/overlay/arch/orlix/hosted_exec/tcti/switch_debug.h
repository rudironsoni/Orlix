/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SWITCH_DEBUG_H
#define ORLIX_TCTI_SWITCH_DEBUG_H

#include <asm/tcti.h>

struct tcti_decoded_instruction;

int tcti_switch_debug_execute_decoded(struct mm_struct *mm,
				      struct pt_regs *regs,
				      const struct tcti_decoded_instruction *decoded,
				      unsigned long *fault_address);
struct tcti_result tcti_switch_debug_resume_user(struct task_struct *task,
						 struct pt_regs *regs,
						 struct mm_struct *mm);

#endif /* ORLIX_TCTI_SWITCH_DEBUG_H */
