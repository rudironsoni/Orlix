/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BARRIER_GADGETS_H
#define ORLIX_TCTI_BARRIER_GADGETS_H

#include "gadget_program.h"

bool orlix_tcti_is_native_barrier_gadget(
	const struct orlix_tcti_decoded_instruction *decoded);
int orlix_tcti_native_barrier_execute(u8 op, u8 option, bool nxs);
void orlix_tcti_native_csdb(void);
int orlix_tcti_gadget_execute_native_barrier(struct mm_struct *mm,
	struct pt_regs *regs, const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address);

#endif
