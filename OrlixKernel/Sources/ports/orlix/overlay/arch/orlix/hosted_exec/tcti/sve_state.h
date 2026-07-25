/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SVE_STATE_H
#define ORLIX_TCTI_SVE_STATE_H

#include <asm/tcti.h>

struct pt_regs;

/*
 * SVE's architectural maximum vector length is 2048 bits.  The state is
 * deliberately stored in the guest task, never borrowed from host SIMD state.
 */
enum tcti_sve_predication {
	TCTI_SVE_PREDICATE_MERGING,
	TCTI_SVE_PREDICATE_ZEROING,
};

enum tcti_sve_integer_binary_op {
	/* AARCHMRS 2026-06: add_z_p_zz_, sub_z_p_zz_, and_z_p_zz_,
	 * orr_z_p_zz_, and eor_z_p_zz_. */
	TCTI_SVE_INTEGER_ADD,
	TCTI_SVE_INTEGER_SUB,
	TCTI_SVE_INTEGER_AND,
	TCTI_SVE_INTEGER_ORR,
	TCTI_SVE_INTEGER_EOR,
};

/* `user_simd` is the authoritative shared V0-V31 low-128-bit backing. */
int tcti_sve_predicated_integer_binary(struct tcti_sve_state *state,
				unsigned long *user_simd,
				enum tcti_sve_integer_binary_op op,
				enum tcti_sve_predication predication,
				u8 zd, u8 pg, u8 zn, u8 zm,
				u8 element_bytes);
int tcti_sve_execute_predicated_integer_binary(struct tcti_sve_state *state,
					struct pt_regs *regs,
					unsigned long *user_simd,
					bool available,
					enum tcti_sve_integer_binary_op op,
					enum tcti_sve_predication predication,
					u8 zd, u8 pg, u8 zn, u8 zm,
					u8 element_bytes);

#endif /* ORLIX_TCTI_SVE_STATE_H */
