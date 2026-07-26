/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/bitops.h>

#include "sve_decode.h"

struct orlix_tcti_sve_integer_binary_decode {
	s8 op;
};

static const struct orlix_tcti_sve_integer_binary_decode
orlix_tcti_sve_integer_binary_ops[32] = {
	[0 ... 31] = { -1 },
	[0] = { ORLIX_TCTI_SVE_INTEGER_ADD }, [1] = { ORLIX_TCTI_SVE_INTEGER_SUB },
	[3] = { ORLIX_TCTI_SVE_INTEGER_SUBR }, [8] = { ORLIX_TCTI_SVE_INTEGER_SMAX },
	[9] = { ORLIX_TCTI_SVE_INTEGER_UMAX }, [10] = { ORLIX_TCTI_SVE_INTEGER_SMIN },
	[11] = { ORLIX_TCTI_SVE_INTEGER_UMIN }, [12] = { ORLIX_TCTI_SVE_INTEGER_SABD },
	[13] = { ORLIX_TCTI_SVE_INTEGER_UABD }, [16] = { ORLIX_TCTI_SVE_INTEGER_MUL },
	[18] = { ORLIX_TCTI_SVE_INTEGER_SMULH }, [19] = { ORLIX_TCTI_SVE_INTEGER_UMULH },
	[20] = { ORLIX_TCTI_SVE_INTEGER_SDIV }, [21] = { ORLIX_TCTI_SVE_INTEGER_UDIV },
	[22] = { ORLIX_TCTI_SVE_INTEGER_SDIVR }, [23] = { ORLIX_TCTI_SVE_INTEGER_UDIVR },
	[24] = { ORLIX_TCTI_SVE_INTEGER_ORR }, [25] = { ORLIX_TCTI_SVE_INTEGER_EOR },
	[26] = { ORLIX_TCTI_SVE_INTEGER_AND }, [27] = { ORLIX_TCTI_SVE_INTEGER_BIC },
};

int orlix_tcti_decode_sve_predicated_integer_binary(
	u32 instruction, struct orlix_tcti_sve_predicated_integer_binary *decoded)
{
	u8 opcode;

	if (!decoded)
		return -EINVAL;

	if ((instruction & AARCH64_SVE_PREDICATED_INTEGER_BINARY_MASK) !=
	    AARCH64_SVE_PREDICATED_INTEGER_BINARY)
		return -ENOENT;

	opcode = (instruction >> 16) & 0x1fU;
	if (orlix_tcti_sve_integer_binary_ops[opcode].op < 0 ||
	    !orlix_tcti_sve_integer_binary_op_supports_element_bytes(
		(enum orlix_tcti_sve_integer_binary_op)
			orlix_tcti_sve_integer_binary_ops[opcode].op,
		1U << ((instruction >> 22) & 0x3U)))
		return -EINVAL;
	decoded->op = (enum orlix_tcti_sve_integer_binary_op)
		orlix_tcti_sve_integer_binary_ops[opcode].op;

	decoded->predication = ORLIX_TCTI_SVE_PREDICATE_MERGING;
	decoded->element_bytes = 1U << ((instruction >> 22) & 0x3U);
	decoded->zd = instruction & 0x1fU;
	decoded->zn = decoded->zd;
	decoded->zm = (instruction >> 5) & 0x1fU;
	decoded->pg = (instruction >> 10) & 0x7U;
	return 0;
}
