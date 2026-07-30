/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_NATIVE_PROOF_REGISTRY_PRIVATE_H
#define ORLIX_TCTI_TARGET_NATIVE_PROOF_REGISTRY_PRIVATE_H

#include "target_native_proof_contract.h"

const void *orlix_tcti_native_proof_registry_capture_token_internal(size_t index);
const void *orlix_tcti_native_proof_registry_capture_token_for_entry(
	const struct orlix_tcti_native_proof_registry_entry *entry);
int orlix_tcti_native_proof_registry_resolve_production_entries(
	const struct orlix_tcti_native_proof_registry_entry *entries, size_t count,
	const void *capture_token, orlix_tcti_proof_u32 source_ordinal,
	orlix_tcti_proof_u64 semantic_variant_identity,
	orlix_tcti_proof_u32 obligation,
	const struct orlix_tcti_native_proof_registry_entry **entry,
	enum orlix_tcti_native_contract_error *error);

#endif
