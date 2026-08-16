/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BARRIER_GADGETS_H
#define ORLIX_TCTI_BARRIER_GADGETS_H

#include <linux/atomic.h>

#include "gadget_program.h"

/*
 * A production-owned completion record.  It advances only after the fixed
 * native barrier instruction returns, so it is suitable for source-bound
 * tests that distinguish dispatched barriers from rejected encodings.
 */
struct orlix_tcti_barrier_observation {
	u64 sequence;
	u8 op;
	u8 option;
	bool nxs;
};

bool orlix_tcti_is_native_barrier_gadget(
	const struct orlix_tcti_decoded_instruction *decoded);
int orlix_tcti_native_barrier_execute(u8 op, u8 option, bool nxs);
int orlix_tcti_native_dmb_ish(void);
#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
void orlix_tcti_native_dmb_ish_set_handoff(atomic_t *handoff);
#endif
void orlix_tcti_native_barrier_observation(
	struct orlix_tcti_barrier_observation *observation);
void orlix_tcti_native_csdb(void);
int orlix_tcti_gadget_execute_native_barrier(struct mm_struct *mm,
	struct pt_regs *regs, const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address);

#endif
