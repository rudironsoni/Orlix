/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SETGO_H
#define ORLIX_TCTI_SETGO_H

struct mm_struct;
struct pt_regs;
struct orlix_tcti_decoded_instruction;

int orlix_tcti_execute_setgo(struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long *fault_address);

#endif /* ORLIX_TCTI_SETGO_H */
