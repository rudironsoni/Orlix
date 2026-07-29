/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BRANCH_CONTROL_H
#define ORLIX_TCTI_BRANCH_CONTROL_H

#include <asm/orlix_tcti.h>

struct orlix_tcti_decoded_instruction;

bool orlix_tcti_branch_control_decoded(
	const struct orlix_tcti_decoded_instruction *decoded);

int orlix_tcti_execute_branch_control_semantics(
	struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_BRANCH_CONTROL_H */
