/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SVE_STATE_H
#define ORLIX_TCTI_SVE_STATE_H

#include <asm/orlix_tcti.h>

struct pt_regs;

/*
 * SVE's architectural maximum vector length is 2048 bits.  The state is
 * deliberately stored in the guest task, never borrowed from host SIMD state.
 */
enum orlix_tcti_sve_predication {
	ORLIX_TCTI_SVE_PREDICATE_MERGING,
	ORLIX_TCTI_SVE_PREDICATE_ZEROING,
};

enum orlix_tcti_sve_integer_binary_op {
	/* AARCHMRS 2026-06 source ordinals 0-2 and 5-21. */
	ORLIX_TCTI_SVE_INTEGER_ADD,
	ORLIX_TCTI_SVE_INTEGER_SUB,
	ORLIX_TCTI_SVE_INTEGER_SUBR,
	ORLIX_TCTI_SVE_INTEGER_SMAX,
	ORLIX_TCTI_SVE_INTEGER_SMIN,
	ORLIX_TCTI_SVE_INTEGER_SABD,
	ORLIX_TCTI_SVE_INTEGER_UMAX,
	ORLIX_TCTI_SVE_INTEGER_UMIN,
	ORLIX_TCTI_SVE_INTEGER_UABD,
	ORLIX_TCTI_SVE_INTEGER_MUL,
	ORLIX_TCTI_SVE_INTEGER_SMULH,
	ORLIX_TCTI_SVE_INTEGER_UMULH,
	ORLIX_TCTI_SVE_INTEGER_SDIV,
	ORLIX_TCTI_SVE_INTEGER_SDIVR,
	ORLIX_TCTI_SVE_INTEGER_UDIV,
	ORLIX_TCTI_SVE_INTEGER_UDIVR,
	ORLIX_TCTI_SVE_INTEGER_AND,
	ORLIX_TCTI_SVE_INTEGER_ORR,
	ORLIX_TCTI_SVE_INTEGER_EOR,
	ORLIX_TCTI_SVE_INTEGER_BIC,
};

/* `user_simd` is the authoritative shared V0-V31 low-128-bit backing. */
bool orlix_tcti_sve_integer_binary_op_supports_element_bytes(
	enum orlix_tcti_sve_integer_binary_op op, u8 element_bytes);
int orlix_tcti_sve_predicated_integer_binary(struct orlix_tcti_sve_state *state,
				unsigned long *user_simd,
				enum orlix_tcti_sve_integer_binary_op op,
				enum orlix_tcti_sve_predication predication,
				u8 zd, u8 pg, u8 zn, u8 zm,
				u8 element_bytes);
int orlix_tcti_sve_execute_predicated_integer_binary(struct orlix_tcti_sve_state *state,
					struct pt_regs *regs,
					unsigned long *user_simd,
					bool available,
					enum orlix_tcti_sve_integer_binary_op op,
					enum orlix_tcti_sve_predication predication,
					u8 zd, u8 pg, u8 zn, u8 zm,
					u8 element_bytes);

#endif /* ORLIX_TCTI_SVE_STATE_H */
