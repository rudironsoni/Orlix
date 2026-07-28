/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_PROOF_INGESTION_H
#define ORLIX_TCTI_TARGET_PROOF_INGESTION_H

#ifndef __KERNEL__
#include <stdbool.h>
#endif

#include "target_proof_registry.h"

#define ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX 192U
#define ORLIX_TCTI_TARGET_PROOF_OBLIGATION_BITS 11U
#define ORLIX_TCTI_TARGET_PROOF_LEDGER_MAX_RECORDS \
	((size_t)ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS * \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_BITS)

enum orlix_tcti_target_native_result_kind {
	ORLIX_TCTI_TARGET_NATIVE_RESULT_INVALID,
	ORLIX_TCTI_TARGET_NATIVE_RESULT_RESULT,
	ORLIX_TCTI_TARGET_NATIVE_RESULT_GPR,
	ORLIX_TCTI_TARGET_NATIVE_RESULT_NON_PRODUCTION,
};

enum orlix_tcti_target_kselftest_result_state {
	ORLIX_TCTI_TARGET_KSELFTEST_UNEXECUTED,
	ORLIX_TCTI_TARGET_KSELFTEST_PASSED,
	ORLIX_TCTI_TARGET_KSELFTEST_FAILED,
	ORLIX_TCTI_TARGET_KSELFTEST_SKIPPED,
	ORLIX_TCTI_TARGET_KSELFTEST_INCOMPLETE,
};

enum orlix_tcti_target_proof_ingestion_error {
	ORLIX_TCTI_TARGET_PROOF_INGEST_OK,
	ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID,
	ORLIX_TCTI_TARGET_PROOF_INGEST_REPLAY,
	ORLIX_TCTI_TARGET_PROOF_INGEST_UNKNOWN,
	ORLIX_TCTI_TARGET_PROOF_INGEST_STALE_SOURCE,
	ORLIX_TCTI_TARGET_PROOF_INGEST_FAMILY_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_CLASSIFICATION_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_CONDITION_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_BUILD_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_SUITE_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_CASE_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_PATH_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_OBLIGATION_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_INGEST_NON_PRODUCTION,
	ORLIX_TCTI_TARGET_PROOF_INGEST_RESULT_NOT_PASSED,
	ORLIX_TCTI_TARGET_PROOF_INGEST_NOT_APPLICABLE,
};

struct orlix_tcti_target_native_result_record;

struct orlix_tcti_target_native_ingestion_selector {
	const char *proof_id;
	orlix_tcti_proof_u32 classification_mask;
	const char *condition_tcnd_hex;
	const char *kunit_source;
	const char *kunit_source_sha256;
	const char *kunit_build_source;
	const char *kunit_build_source_sha256;
	const char *kunit_suite;
	const char *kunit_case;
	const char *executing_kernel_identity;
};

struct orlix_tcti_target_kselftest_result;
struct orlix_tcti_target_proof_ingestion_ledger;

struct orlix_tcti_target_proof_ingestion_summary {
	size_t accepted_records;
	size_t native_passed;
	size_t kselftest_passed;
	size_t rejected;
};

struct orlix_tcti_target_kunit_provenance_identity {
	const char *source;
	const char *source_sha256;
	const char *build_source;
	const char *build_source_sha256;
	const char *suite;
	const char *case_name;
};

int orlix_tcti_target_kunit_provenance_identity(
	const struct orlix_tcti_target_proof_registry_entry *entry,
	const char *case_name,
	struct orlix_tcti_target_kunit_provenance_identity *identity);

void orlix_tcti_target_native_result_record_destroy(
	struct orlix_tcti_target_native_result_record *record);
struct orlix_tcti_target_proof_ingestion_ledger *
orlix_tcti_target_proof_ingestion_ledger_create(size_t capacity);
void orlix_tcti_target_proof_ingestion_ledger_destroy(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger);
int orlix_tcti_target_proof_ingestion_summary(
	const struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_target_proof_ingestion_summary *summary);

int orlix_tcti_target_proof_ingest_native(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_target_native_result_record *record,
	const struct orlix_tcti_target_native_ingestion_selector *selector,
	enum orlix_tcti_target_proof_ingestion_error *error);
int orlix_tcti_target_proof_ingest_kselftest(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	const struct orlix_tcti_target_linux_proof_disposition_row *requested_row,
	const struct orlix_tcti_target_kselftest_result *result,
	const char *executing_kernel_identity,
	enum orlix_tcti_target_proof_ingestion_error *error);

#endif /* ORLIX_TCTI_TARGET_PROOF_INGESTION_H */
