/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_NATIVE_PROOF_CONTRACT_PRIVATE_H
#define ORLIX_TCTI_TARGET_NATIVE_PROOF_CONTRACT_PRIVATE_H

#include "target_native_proof_contract.h"

struct orlix_tcti_native_wire_record {
	const orlix_tcti_proof_u8 *bytes;
	size_t length;
	orlix_tcti_proof_u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE];
	orlix_tcti_proof_u64 identity;
	/* Immutable producer handle. Ingestion resolves this before reading bytes. */
	orlix_tcti_proof_u64 producer_nonce;
	orlix_tcti_proof_u64 producer_binding;
	orlix_tcti_proof_u8 sealed;
	orlix_tcti_proof_u8 production_origin;
	orlix_tcti_proof_u8 consumed;
};

struct orlix_tcti_native_wire_header {
	orlix_tcti_proof_u64 declaration_identity;
	orlix_tcti_proof_u64 registry_identity;
	orlix_tcti_proof_u64 source_identity;
	orlix_tcti_proof_u64 execution_identity;
	orlix_tcti_proof_u64 registry_binding;
	orlix_tcti_proof_u64 finalization_nonce;
	orlix_tcti_proof_u32 obligation;
	orlix_tcti_proof_u32 record_kind;
};

bool orlix_tcti_native_wire_record_has_production_origin(
	const struct orlix_tcti_native_wire_record *record);
int orlix_tcti_native_wire_record_validate(
	const struct orlix_tcti_native_wire_record *record,
	const struct orlix_tcti_native_capture_declaration *declaration,
	orlix_tcti_proof_u64 registry_identity);
int orlix_tcti_native_wire_record_header(
	const struct orlix_tcti_native_wire_record *record,
	struct orlix_tcti_native_wire_header *header);
void orlix_tcti_native_wire_record_destroy(
	struct orlix_tcti_native_wire_record *record);

#endif /* ORLIX_TCTI_TARGET_NATIVE_PROOF_CONTRACT_PRIVATE_H */
