/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SYSTEM_ACCESSOR_H
#define ORLIX_TCTI_SYSTEM_ACCESSOR_H

#include <linux/types.h>

struct pt_regs;
struct orlix_tcti_decoded_instruction;

/* The generated reconciliation ledger is the sole selector acceptance source. */
bool orlix_tcti_system_accessor_decode(
	u16 selector, bool write, struct orlix_tcti_decoded_instruction *decoded);
int orlix_tcti_execute_system_register(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_SYSTEM_ACCESSOR_H */
