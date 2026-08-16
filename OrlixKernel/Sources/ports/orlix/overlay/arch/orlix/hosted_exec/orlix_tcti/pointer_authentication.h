/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_POINTER_AUTHENTICATION_H
#define ORLIX_TCTI_POINTER_AUTHENTICATION_H

#include <linux/types.h>

#include "decode_aarch64.h"

struct mm_struct;
struct pt_regs;
struct task_struct;
struct orlix_tcti_pauth_key;

/* Orlix's A64 guest profile uses a 48-bit VA and QARMA5 without TBI/PAuth2. */
#define ORLIX_TCTI_PAUTH_BOTTOM_BIT 48U

u64 orlix_tcti_pauth_compute_qarma5(u64 data, u64 modifier,
				    const struct orlix_tcti_pauth_key *key);
u64 orlix_tcti_pauth_compute_qarma5_two_modifiers(
	u64 data, u64 modifier1, u64 modifier2,
	const struct orlix_tcti_pauth_key *key);
u64 orlix_tcti_pauth_add(u64 pointer, u64 modifier, u64 modifier2,
			 bool use_modifier2,
			 const struct orlix_tcti_pauth_key *key);
u64 orlix_tcti_pauth_authenticate(u64 pointer, u64 modifier, u64 modifier2,
				  bool use_modifier2, bool key_b,
				  const struct orlix_tcti_pauth_key *key);
u64 orlix_tcti_pauth_strip(u64 pointer);

void orlix_tcti_pauth_randomize_task(struct task_struct *task);
void orlix_tcti_pauth_copy_task(struct task_struct *destination,
				const struct task_struct *source);
void orlix_tcti_pauth_clear_task(struct task_struct *task);

int orlix_tcti_execute_pointer_authentication(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long *fault_address);

#endif /* ORLIX_TCTI_POINTER_AUTHENTICATION_H */
