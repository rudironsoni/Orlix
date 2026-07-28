/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_PROOF_INGESTION_PRIVATE_H
#define ORLIX_TCTI_TARGET_PROOF_INGESTION_PRIVATE_H

#ifdef __KERNEL__
#include <linux/atomic.h>
#else
#include <stdatomic.h>
#endif

#include "target_proof_ingestion.h"

#define ORLIX_TCTI_TARGET_NATIVE_RECORD_MAGIC 0x5450524eU
#define ORLIX_TCTI_TARGET_NATIVE_TEXT_MAX 256U
#define ORLIX_TCTI_TARGET_SHA256_HEX_LENGTH 64U

/* C-private layout. Production construction exists only in native_observation.c. */
struct orlix_tcti_target_native_result_record {
	orlix_tcti_proof_u32 magic;
	orlix_tcti_proof_u32 source_ordinal;
	orlix_tcti_proof_u32 encoding_mask;
	orlix_tcti_proof_u32 encoding_pattern;
	orlix_tcti_proof_u32 entry_instruction;
	enum orlix_tcti_target_native_result_kind kind;
	orlix_tcti_proof_u32 legal_encoding_count;
	orlix_tcti_proof_u32 rejected_encoding_count;
	orlix_tcti_proof_u64 encoding_domain_digest;
	bool production_resume;
	bool source_bound;
	bool match;
	orlix_tcti_proof_u32 resume_count;
	char leaf_name[ORLIX_TCTI_TARGET_NATIVE_TEXT_MAX];
	char mnemonic[ORLIX_TCTI_TARGET_NATIVE_TEXT_MAX];
	char operation_id[ORLIX_TCTI_TARGET_NATIVE_TEXT_MAX];
	char artifact_architecture[64];
	char artifact_build[64];
	char artifact_reference[64];
	char artifact_schema[64];
	char artifact_source_sha256[ORLIX_TCTI_TARGET_SHA256_HEX_LENGTH + 1U];
	char executing_kernel_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	char implementation_owner[ORLIX_TCTI_TARGET_NATIVE_TEXT_MAX];
	char decoder_owner[ORLIX_TCTI_TARGET_NATIVE_TEXT_MAX];
	char lowering_owner[ORLIX_TCTI_TARGET_NATIVE_TEXT_MAX];
	orlix_tcti_proof_u64 identity;
#ifdef __KERNEL__
	atomic_t consumed;
#else
	atomic_bool consumed;
#endif
};

struct orlix_tcti_target_kselftest_result {
	struct orlix_tcti_target_kselftest_provenance identity;
	enum orlix_tcti_target_kselftest_result_state state;
	char executing_kernel_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	orlix_tcti_proof_u64 identity_hash;
};

struct orlix_tcti_target_proof_ingestion_slot {
	orlix_tcti_proof_u64 identity;
	const void *record;
};

struct orlix_tcti_target_proof_ingestion_ledger {
	struct orlix_tcti_target_proof_ingestion_slot *slots;
	size_t capacity;
	size_t count;
	size_t native_passed;
	size_t kselftest_passed;
	size_t rejected;
};

#ifdef ORLIX_TCTI_PROOF_INGESTION_HOST_TEST
struct orlix_tcti_target_native_result_test_input {
	orlix_tcti_proof_u32 source_ordinal;
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_proof_u32 encoding_mask;
	orlix_tcti_proof_u32 encoding_pattern;
	orlix_tcti_proof_u32 entry_instruction;
	enum orlix_tcti_target_native_result_kind kind;
	orlix_tcti_proof_u32 legal_encoding_count;
	orlix_tcti_proof_u32 rejected_encoding_count;
	orlix_tcti_proof_u64 encoding_domain_digest;
	const char *artifact_architecture;
	const char *artifact_build;
	const char *artifact_reference;
	const char *artifact_schema;
	const char *artifact_source_sha256;
	const char *executing_kernel_identity;
	const char *implementation_owner;
	const char *decoder_owner;
	const char *lowering_owner;
	bool production_resume;
	bool source_bound;
	bool match;
	orlix_tcti_proof_u32 resume_count;
};

struct orlix_tcti_target_native_result_record *
orlix_tcti_target_native_result_record_create_for_test(
	const struct orlix_tcti_target_native_result_test_input *input);
struct orlix_tcti_target_kselftest_result *
orlix_tcti_target_kselftest_result_create_for_test(
	const struct orlix_tcti_target_kselftest_provenance *identity,
	enum orlix_tcti_target_kselftest_result_state state,
	const char *executing_kernel_identity);
void orlix_tcti_target_kselftest_result_destroy_for_test(
	struct orlix_tcti_target_kselftest_result *result);
#endif

#endif /* ORLIX_TCTI_TARGET_PROOF_INGESTION_PRIVATE_H */
