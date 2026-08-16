/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BASE_SYSTEM_139_GADGETS_H
#define ORLIX_TCTI_BASE_SYSTEM_139_GADGETS_H

#include "gadget_program.h"

bool orlix_tcti_is_base_system_139_gadget(
	const struct orlix_tcti_decoded_instruction *decoded);
int orlix_tcti_gadget_execute_base_system_139(struct mm_struct *mm,
	struct pt_regs *regs, const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address);

#endif /* ORLIX_TCTI_BASE_SYSTEM_139_GADGETS_H */
