// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <linux/errno.h>

#include "branch_control.h"
#include "decode_aarch64.h"

bool orlix_tcti_branch_control_decoded(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return false;

	return decoded->decode_class == ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE ||
		decoded->decode_class == ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER;
}

int orlix_tcti_execute_branch_control_semantics(
	struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 target;

	if (!regs || !decoded || !orlix_tcti_branch_control_decoded(decoded))
		return -EINVAL;

	if (decoded->decode_class == ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE) {
		if (decoded->link)
			regs->regs[30] = regs->pc + sizeof(u32);
		regs->pc += decoded->branch_imm;
		return 0;
	}

	/* BR, BLR, and RET use XZR, never SP, for Rn == 31. */
	target = decoded->rn == 31 ? 0 : regs->regs[decoded->rn];
	if (decoded->branch_register_op == ORLIX_TCTI_BRANCH_REGISTER_BLR)
		regs->regs[30] = regs->pc + sizeof(u32);
	regs->pc = target;
	return 0;
}
