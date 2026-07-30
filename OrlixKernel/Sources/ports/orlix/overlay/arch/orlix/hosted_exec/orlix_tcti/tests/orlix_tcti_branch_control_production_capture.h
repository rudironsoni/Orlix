/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BRANCH_CONTROL_PRODUCTION_CAPTURE_H
#define ORLIX_TCTI_BRANCH_CONTROL_PRODUCTION_CAPTURE_H

#include "target_native_proof_registry_private.h"

static inline const void *
orlix_tcti_branch_control_production_capture_token(u32 source_ordinal)
{
	switch (source_ordinal) {
	case 2227U:
		return orlix_tcti_native_proof_registry_capture_token_internal(0U);
	case 2230U:
		return orlix_tcti_native_proof_registry_capture_token_internal(1U);
	default:
		return NULL;
	}
}

#endif
