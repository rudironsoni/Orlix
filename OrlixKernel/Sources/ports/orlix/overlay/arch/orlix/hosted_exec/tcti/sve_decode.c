/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>

#include "sve_decode.h"

static int tcti_sve_decode_arithmetic(u8 opcode,
				      enum tcti_sve_integer_binary_op *op)
{
	switch (opcode) {
	case 0:
		*op = TCTI_SVE_INTEGER_ADD;
		return 0;
	case 1:
		*op = TCTI_SVE_INTEGER_SUB;
		return 0;
	default:
		return -EINVAL;
	}
}

static int tcti_sve_decode_logical(u8 opcode,
				   enum tcti_sve_integer_binary_op *op)
{
	switch (opcode) {
	case 0:
		*op = TCTI_SVE_INTEGER_ORR;
		return 0;
	case 1:
		*op = TCTI_SVE_INTEGER_EOR;
		return 0;
	case 2:
		*op = TCTI_SVE_INTEGER_AND;
		return 0;
	default:
		return -EINVAL;
	}
}

int tcti_decode_sve_predicated_integer_binary(
	u32 instruction, struct tcti_sve_predicated_integer_binary *decoded)
{
	u32 class;
	u8 opcode;
	int ret;

	if (!decoded)
		return -EINVAL;

	class = instruction & AARCH64_SVE_PREDICATED_BINARY_MASK;
	if (class != AARCH64_SVE_PREDICATED_ARITHMETIC &&
	    class != AARCH64_SVE_PREDICATED_LOGICAL)
		return -ENOENT;

	opcode = (instruction >> 16) & 0x7U;
	if (class == AARCH64_SVE_PREDICATED_ARITHMETIC)
		ret = tcti_sve_decode_arithmetic(opcode, &decoded->op);
	else
		ret = tcti_sve_decode_logical(opcode, &decoded->op);
	if (ret)
		return ret;

	decoded->predication = TCTI_SVE_PREDICATE_MERGING;
	decoded->element_bytes = 1U << ((instruction >> 22) & 0x3U);
	decoded->zd = instruction & 0x1fU;
	decoded->zn = decoded->zd;
	decoded->zm = (instruction >> 5) & 0x1fU;
	decoded->pg = (instruction >> 10) & 0x7U;
	return 0;
}
