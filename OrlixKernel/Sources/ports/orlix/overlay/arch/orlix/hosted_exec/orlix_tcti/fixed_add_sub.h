/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FIXED_ADD_SUB_H
#define ORLIX_TCTI_FIXED_ADD_SUB_H

struct pt_regs;
struct orlix_tcti_decoded_instruction;

int orlix_tcti_fixed_execute_add_sub(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_FIXED_ADD_SUB_H */
