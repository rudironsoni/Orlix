/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SEMANTICS_H
#define ORLIX_TCTI_SEMANTICS_H

#include <asm/tcti.h>

struct tcti_decoded_instruction;

int tcti_execute_decoded_semantics(struct mm_struct *mm,
				   struct pt_regs *regs,
				   const struct tcti_decoded_instruction *decoded,
				   unsigned long *fault_address);

#endif /* ORLIX_TCTI_SEMANTICS_H */
