/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/bitops.h>

#include "sve_decode.h"

struct tcti_sve_integer_binary_decode {
	s8 op;
};

static const struct tcti_sve_integer_binary_decode
tcti_sve_integer_binary_ops[32] = {
	[0 ... 31] = { -1 },
	[0] = { TCTI_SVE_INTEGER_ADD }, [1] = { TCTI_SVE_INTEGER_SUB },
	[3] = { TCTI_SVE_INTEGER_SUBR }, [8] = { TCTI_SVE_INTEGER_SMAX },
	[9] = { TCTI_SVE_INTEGER_UMAX }, [10] = { TCTI_SVE_INTEGER_SMIN },
	[11] = { TCTI_SVE_INTEGER_UMIN }, [12] = { TCTI_SVE_INTEGER_SABD },
	[13] = { TCTI_SVE_INTEGER_UABD }, [16] = { TCTI_SVE_INTEGER_MUL },
	[18] = { TCTI_SVE_INTEGER_SMULH }, [19] = { TCTI_SVE_INTEGER_UMULH },
	[20] = { TCTI_SVE_INTEGER_SDIV }, [21] = { TCTI_SVE_INTEGER_UDIV },
	[22] = { TCTI_SVE_INTEGER_SDIVR }, [23] = { TCTI_SVE_INTEGER_UDIVR },
	[24] = { TCTI_SVE_INTEGER_ORR }, [25] = { TCTI_SVE_INTEGER_EOR },
	[26] = { TCTI_SVE_INTEGER_AND }, [27] = { TCTI_SVE_INTEGER_BIC },
};

int tcti_decode_sve_predicated_integer_binary(
	u32 instruction, struct tcti_sve_predicated_integer_binary *decoded)
{
	u8 opcode;

	if (!decoded)
		return -EINVAL;

	if ((instruction & AARCH64_SVE_PREDICATED_INTEGER_BINARY_MASK) !=
	    AARCH64_SVE_PREDICATED_INTEGER_BINARY)
		return -ENOENT;

	opcode = (instruction >> 16) & 0x1fU;
	if (tcti_sve_integer_binary_ops[opcode].op < 0 ||
	    !tcti_sve_integer_binary_op_supports_element_bytes(
		(enum tcti_sve_integer_binary_op)
			tcti_sve_integer_binary_ops[opcode].op,
		1U << ((instruction >> 22) & 0x3U)))
		return -EINVAL;
	decoded->op = (enum tcti_sve_integer_binary_op)
		tcti_sve_integer_binary_ops[opcode].op;

	decoded->predication = TCTI_SVE_PREDICATE_MERGING;
	decoded->element_bytes = 1U << ((instruction >> 22) & 0x3U);
	decoded->zd = instruction & 0x1fU;
	decoded->zn = decoded->zd;
	decoded->zm = (instruction >> 5) & 0x1fU;
	decoded->pg = (instruction >> 10) & 0x7U;
	return 0;
}
