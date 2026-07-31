/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SYSTEM_ACCESSOR_H
#define ORLIX_TCTI_SYSTEM_ACCESSOR_H

#include <linux/types.h>

struct pt_regs;
struct orlix_tcti_decoded_instruction;

enum orlix_tcti_system_accessor_route {
	ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MRS,
	ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MSR,
	ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_SYS,
	ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_SYSL,
	ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MSRR,
	ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MRRS,
};

/* The generated reconciliation ledger is the sole selector acceptance source. */
bool orlix_tcti_system_accessor_decode(
	u16 selector, bool write, struct orlix_tcti_decoded_instruction *decoded);
bool orlix_tcti_system_accessor_decode_route(
	u16 selector, enum orlix_tcti_system_accessor_route route,
	struct orlix_tcti_decoded_instruction *decoded);
int orlix_tcti_execute_system_register(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_SYSTEM_ACCESSOR_H */
