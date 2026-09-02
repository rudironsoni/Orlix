/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BASE_SYSTEM_PRODUCTION_CAPTURE_H
#define ORLIX_TCTI_BASE_SYSTEM_PRODUCTION_CAPTURE_H

#include "target_native_proof_registry_private.h"
#include "target_proof_registry.h"

static inline const void *
orlix_tcti_base_system_production_capture_token(u32 source_ordinal,
						u32 obligation)
{
	const struct orlix_tcti_target_production_capture_binding *bindings;
	size_t count;
	size_t index;

	bindings = orlix_tcti_target_production_capture_bindings(&count);
	if (!bindings)
		return NULL;
	for (index = 0; index < count; index++)
		if (bindings[index].source_ordinal == source_ordinal &&
		    bindings[index].obligation == obligation)
			return orlix_tcti_native_proof_registry_capture_token_internal(
				index);
	return NULL;
}

#endif
