/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FIXED_INTEGER_H
#define ORLIX_TCTI_FIXED_INTEGER_H

#include <asm/orlix_tcti.h>

struct orlix_tcti_decoded_instruction;

int orlix_tcti_fixed_integer_execute(struct pt_regs *regs,
		const struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_FIXED_INTEGER_H */
