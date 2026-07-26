/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Pinned Arm AARCHMRS 2026-06 ordinal-range audit.
 *
 * This is range-coverage metadata only. It binds source_manifest.def directly
 * and deliberately does not duplicate the per-leaf classification and proof
 * truth owned by the canonical completion audit.
 */
#ifndef __ORLIX_TCTI_TARGET_ORDINAL_LEDGER_H
#define __ORLIX_TCTI_TARGET_ORDINAL_LEDGER_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
typedef uint32_t u32;
#endif

#define ORLIX_TCTI_A64_TARGET_ORDINAL_LEAF_COUNT	4350U

struct orlix_tcti_a64_ordinal_range {
	u32 first;
	u32 last;
};

struct orlix_tcti_a64_ordinal_ledger_result {
	u32 rows;
	u32 first_ordinal;
	u32 last_ordinal;
};

/* Validates one contiguous interval in the pinned authoritative source. */
int orlix_tcti_a64_ordinal_ledger_validate(
	const struct orlix_tcti_a64_ordinal_range *range,
	struct orlix_tcti_a64_ordinal_ledger_result *result);

#endif /* __ORLIX_TCTI_TARGET_ORDINAL_LEDGER_H */
