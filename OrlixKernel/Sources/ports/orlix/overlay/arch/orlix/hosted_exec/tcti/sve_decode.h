/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SVE_DECODE_H
#define ORLIX_TCTI_SVE_DECODE_H

#include <linux/types.h>

#include "sve_state.h"

/* AARCHMRS 2026-06 encodesets for the five owned *_z_p_zz_ leaves. */
#define AARCH64_SVE_PREDICATED_BINARY_MASK	0xff38e000U
#define AARCH64_SVE_PREDICATED_ARITHMETIC	0x04000000U
#define AARCH64_SVE_PREDICATED_LOGICAL		0x04180000U

/*
 * AARCHMRS 2026-06 source leaves add_z_p_zz_, sub_z_p_zz_, orr_z_p_zz_,
 * eor_z_p_zz_, and and_z_p_zz_.  Each is merging-predicated and uses Zdn as
 * both the destination and first source.
 */
struct tcti_sve_predicated_integer_binary {
	enum tcti_sve_integer_binary_op op;
	enum tcti_sve_predication predication;
	u8 zd;
	u8 zn;
	u8 zm;
	u8 pg;
	u8 element_bytes;
};

/*
 * Returns zero for one of the five owned source leaves, -ENOENT when the
 * instruction is outside their encoding space, or -EINVAL for a reserved
 * opcode in that encoding space.  The central TCTI decoder must call this
 * before its unsupported fallthrough, then dispatch a successful descriptor
 * to tcti_sve_execute_predicated_integer_binary().
 */
int tcti_decode_sve_predicated_integer_binary(
	u32 instruction, struct tcti_sve_predicated_integer_binary *decoded);

#endif /* ORLIX_TCTI_SVE_DECODE_H */
