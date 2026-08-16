/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SME_FP_H
#define ORLIX_TCTI_SME_FP_H

struct pt_regs;
struct orlix_tcti_decoded_instruction;

int orlix_tcti_execute_sme_pstate(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded);
int orlix_tcti_fixed_execute_sme_fp(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded);
int orlix_tcti_execute_sme_fp(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_SME_FP_H */
