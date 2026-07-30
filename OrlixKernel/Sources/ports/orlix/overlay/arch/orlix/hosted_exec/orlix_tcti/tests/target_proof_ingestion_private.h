/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_PROOF_INGESTION_PRIVATE_H
#define ORLIX_TCTI_TARGET_PROOF_INGESTION_PRIVATE_H

#ifdef __KERNEL__
#include <linux/atomic.h>
#include <linux/mutex.h>
#else
#include <stdatomic.h>
#include <pthread.h>
#endif

#include "target_proof_ingestion.h"

#define ORLIX_TCTI_TARGET_SHA256_HEX_LENGTH 64U

enum orlix_tcti_target_native_capture_origin {
	ORLIX_TCTI_TARGET_NATIVE_CAPTURE_TEST_FIXTURE = 1,
	ORLIX_TCTI_TARGET_NATIVE_CAPTURE_PRODUCTION_RESUME,
};

struct orlix_tcti_target_kselftest_result {
	orlix_tcti_proof_u32 capture_origin;
	struct {
		char source[ORLIX_TCTI_NATIVE_TEXT_MAX];
		char source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
		char build_source[ORLIX_TCTI_NATIVE_TEXT_MAX];
		char build_source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
		char program[ORLIX_TCTI_NATIVE_TEXT_MAX];
		char case_name[ORLIX_TCTI_NATIVE_TEXT_MAX];
	} identity;
	size_t row_ordinal;
	struct orlix_tcti_native_source_identity subject;
	enum orlix_tcti_target_kselftest_result_state state;
	char executing_kernel_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	orlix_tcti_proof_u32 disposition;
	orlix_tcti_proof_u32 not_applicable_reason;
	orlix_tcti_proof_u32 linux_owner_mask;
	orlix_tcti_proof_u32 execution_state;
	char kernel_archive_input_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char instruction_artifact_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kernel_config_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char build_profile_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char durable_source_revision[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	orlix_tcti_proof_u64 instruction_artifact_identity;
	orlix_tcti_proof_u64 identity_hash;
	#ifdef __KERNEL__
	atomic_t consumed;
	#else
	atomic_bool consumed;
	#endif
};

struct orlix_tcti_target_kselftest_replay_key {
	orlix_tcti_proof_u64 identity_hash;
	size_t row_ordinal;
	struct orlix_tcti_native_source_identity subject;
	char source[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char build_source[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char build_source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char program[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char case_name[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char executing_kernel_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	char kernel_archive_input_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char instruction_artifact_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kernel_config_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char build_profile_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char durable_source_revision[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	orlix_tcti_proof_u64 instruction_artifact_identity;
	orlix_tcti_proof_u32 state;
	orlix_tcti_proof_u32 disposition;
	orlix_tcti_proof_u32 not_applicable_reason;
	orlix_tcti_proof_u32 linux_owner_mask;
	orlix_tcti_proof_u32 execution_state;
	orlix_tcti_proof_u32 capture_origin;
};

struct orlix_tcti_target_proof_ingestion_slot {
	orlix_tcti_proof_u64 identity;
	orlix_tcti_proof_u64 registry_identity;
	orlix_tcti_proof_u32 source_ordinal;
	orlix_tcti_proof_u64 semantic_variant_identity;
	orlix_tcti_proof_u32 obligation;
	char proof_id[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_suite[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_case[ORLIX_TCTI_NATIVE_TEXT_MAX];
	struct orlix_tcti_target_kselftest_replay_key kselftest_key;
	bool native;
};

struct orlix_tcti_target_proof_ingestion_ledger {
	/* The ledger and record consumed bit commit as one transaction. */
#ifdef __KERNEL__
	struct mutex lock;
#else
	pthread_mutex_t lock;
#endif
	struct orlix_tcti_target_proof_ingestion_slot *slots;
	size_t capacity;
	size_t count;
	size_t native_passed;
	size_t kselftest_passed;
	size_t rejected;
};

#ifdef ORLIX_TCTI_PROOF_INGESTION_HOST_TEST
struct orlix_tcti_target_kselftest_result *
orlix_tcti_target_kselftest_result_create_for_test(
	const struct orlix_tcti_target_linux_proof_disposition_row *row,
	const struct orlix_tcti_target_kselftest_provenance *identity,
	enum orlix_tcti_target_kselftest_result_state state,
	const char *executing_kernel_identity);
void orlix_tcti_target_kselftest_result_destroy_for_test(
	struct orlix_tcti_target_kselftest_result *result);
#endif

#endif /* ORLIX_TCTI_TARGET_PROOF_INGESTION_PRIVATE_H */
