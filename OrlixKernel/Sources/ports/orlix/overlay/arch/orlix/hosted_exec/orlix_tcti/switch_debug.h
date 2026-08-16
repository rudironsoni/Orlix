/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SWITCH_DEBUG_H
#define ORLIX_TCTI_SWITCH_DEBUG_H

#include <asm/orlix_tcti.h>

struct orlix_tcti_decoded_instruction;

int orlix_tcti_switch_debug_execute_decoded(struct mm_struct *mm,
				      struct pt_regs *regs,
				      const struct orlix_tcti_decoded_instruction *decoded,
				      unsigned long *fault_address);
int orlix_tcti_stgp_read_tuple(struct mm_struct *mm,
			       unsigned long tagged_address,
			       void *buffer, size_t size, u8 *tag);
struct orlix_tcti_result orlix_tcti_switch_debug_resume_user(struct task_struct *task,
						 struct pt_regs *regs,
						 struct mm_struct *mm);

#endif /* ORLIX_TCTI_SWITCH_DEBUG_H */
