/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SME_FP_DECODE_H
#define ORLIX_TCTI_SME_FP_DECODE_H

#include <linux/types.h>

struct orlix_tcti_decoded_instruction;

bool orlix_tcti_sme_fp_source_ordinal(u16 ordinal);
int orlix_tcti_decode_sme_fp(u32 instruction,
			     struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_SME_FP_DECODE_H */
