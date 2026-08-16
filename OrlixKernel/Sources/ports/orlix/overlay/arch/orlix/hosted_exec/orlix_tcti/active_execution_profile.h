/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_H
#define ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_H

#include <linux/types.h>

struct orlix_tcti_decoded_instruction;

enum orlix_tcti_active_execution_feature_state {
	ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED = 0,
	ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_ENABLED,
	ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_INVALID,
};

enum orlix_tcti_active_execution_feature_state
orlix_tcti_active_execution_profile_feature(const char *feature_name);

/* This is the sole image-local authorization boundary for decoded optional
 * extensions. Linux AUXV advertisement remains independently fail-closed. */
bool orlix_tcti_active_execution_profile_allows_decoded(
	const struct orlix_tcti_decoded_instruction *decoded);
#endif
