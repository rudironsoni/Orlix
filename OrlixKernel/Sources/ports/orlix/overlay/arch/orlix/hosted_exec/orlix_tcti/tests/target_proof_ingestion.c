// SPDX-License-Identifier: GPL-2.0-only
#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#define proof_alloc(size) kzalloc((size), GFP_KERNEL)
#define proof_free(pointer) kfree(pointer)
#else
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define proof_alloc(size) calloc(1, (size))
#define proof_free(pointer) free(pointer)
#endif

#include "target_proof_ingestion.h"
#include "target_proof_ingestion_private.h"
#include "target_instruction_artifact.h"

static bool empty(const char *text)
{
	return !text || !text[0];
}

#ifdef ORLIX_TCTI_PROOF_INGESTION_HOST_TEST
static bool copy_text(char *destination, size_t capacity, const char *source)
{
	size_t length;

	if (!destination || !capacity || empty(source))
		return false;
	length = strlen(source);
	if (length >= capacity)
		return false;
	memcpy(destination, source, length + 1U);
	return true;
}

static bool sha256_text(const char *text)
{
	size_t index;

	if (!text || strlen(text) != ORLIX_TCTI_TARGET_SHA256_HEX_LENGTH)
		return false;
	for (index = 0; index < ORLIX_TCTI_TARGET_SHA256_HEX_LENGTH; index++)
		if (!((text[index] >= '0' && text[index] <= '9') ||
		      (text[index] >= 'a' && text[index] <= 'f')))
			return false;
	return true;
}

static orlix_tcti_proof_u64 hash_bytes(orlix_tcti_proof_u64 value,
				       const void *bytes, size_t size)
{
	const unsigned char *cursor = bytes;
	size_t index;

	for (index = 0; index < size; index++) {
		value ^= cursor[index];
		value *= ORLIX_TCTI_PROOF_U64_C(1099511628211);
	}
	return value;
}

static orlix_tcti_proof_u64 native_identity(
	const struct orlix_tcti_target_native_result_record *record)
{
	orlix_tcti_proof_u64 value = ORLIX_TCTI_PROOF_U64_C(1469598103934665603);

	value = hash_bytes(value, &record->source_ordinal,
			   sizeof(record->source_ordinal));
	value = hash_bytes(value, &record->entry_instruction,
			   sizeof(record->entry_instruction));
	value = hash_bytes(value, &record->kind, sizeof(record->kind));
	value = hash_bytes(value, record->artifact_source_sha256,
			   strlen(record->artifact_source_sha256));
	value = hash_bytes(value, record->executing_kernel_identity,
			   strlen(record->executing_kernel_identity));
	return value ? value : 1U;
}

struct orlix_tcti_target_native_result_record *
orlix_tcti_target_native_result_record_create_for_test(
	const struct orlix_tcti_target_native_result_test_input *input)
{
	struct orlix_tcti_target_native_result_record *record;

	if (!input || input->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_INVALID ||
	    !sha256_text(input->artifact_source_sha256))
		return NULL;
	record = proof_alloc(sizeof(*record));
	if (!record)
		return NULL;
	atomic_init(&record->consumed, false);
	record->source_ordinal = input->source_ordinal;
	record->encoding_mask = input->encoding_mask;
	record->encoding_pattern = input->encoding_pattern;
	record->entry_instruction = input->entry_instruction;
	record->kind = input->kind;
	record->production_resume = input->production_resume;
	record->source_bound = input->source_bound;
	record->match = input->match;
	record->resume_count = input->resume_count;
	if (!copy_text(record->leaf_name, sizeof(record->leaf_name), input->leaf_name) ||
	    !copy_text(record->mnemonic, sizeof(record->mnemonic), input->mnemonic) ||
	    !copy_text(record->operation_id, sizeof(record->operation_id), input->operation_id) ||
	    !copy_text(record->artifact_architecture,
		       sizeof(record->artifact_architecture), input->artifact_architecture) ||
	    !copy_text(record->artifact_build, sizeof(record->artifact_build),
		       input->artifact_build) ||
	    !copy_text(record->artifact_reference, sizeof(record->artifact_reference),
		       input->artifact_reference) ||
	    !copy_text(record->artifact_schema, sizeof(record->artifact_schema),
		       input->artifact_schema) ||
	    !copy_text(record->artifact_source_sha256,
		       sizeof(record->artifact_source_sha256),
		       input->artifact_source_sha256) ||
	    !copy_text(record->executing_kernel_identity,
		       sizeof(record->executing_kernel_identity),
		       input->executing_kernel_identity) ||
	    !copy_text(record->implementation_owner,
		       sizeof(record->implementation_owner), input->implementation_owner) ||
	    !copy_text(record->decoder_owner, sizeof(record->decoder_owner),
		       input->decoder_owner) ||
	    !copy_text(record->lowering_owner, sizeof(record->lowering_owner),
		       input->lowering_owner)) {
		proof_free(record);
		return NULL;
	}
	record->identity = native_identity(record);
	record->magic = ORLIX_TCTI_TARGET_NATIVE_RECORD_MAGIC;
	return record;
}

struct orlix_tcti_target_kselftest_result *
orlix_tcti_target_kselftest_result_create_for_test(
	const struct orlix_tcti_target_kselftest_provenance *identity,
	enum orlix_tcti_target_kselftest_result_state state,
	const char *executing_kernel_identity)
{
	struct orlix_tcti_target_kselftest_result *result;

	if (!identity || empty(executing_kernel_identity))
		return NULL;
	result = proof_alloc(sizeof(*result));
	if (!result)
		return NULL;
	result->identity = *identity;
	result->state = state;
	if (!copy_text(result->executing_kernel_identity,
		       sizeof(result->executing_kernel_identity),
		       executing_kernel_identity)) {
		proof_free(result);
		return NULL;
	}
	result->identity_hash = ORLIX_TCTI_PROOF_U64_C(1469598103934665603);
	result->identity_hash = hash_bytes(result->identity_hash, identity->source,
					 strlen(identity->source));
	result->identity_hash = hash_bytes(result->identity_hash,
		identity->source_sha256, strlen(identity->source_sha256));
	result->identity_hash = hash_bytes(result->identity_hash, identity->program,
					 strlen(identity->program));
	result->identity_hash = hash_bytes(result->identity_hash, identity->case_name,
					 strlen(identity->case_name));
	result->identity_hash = hash_bytes(result->identity_hash,
		result->executing_kernel_identity,
		strlen(result->executing_kernel_identity));
	if (!result->identity_hash)
		result->identity_hash = 1;
	return result;
}

void orlix_tcti_target_kselftest_result_destroy_for_test(
	struct orlix_tcti_target_kselftest_result *result)
{
	proof_free(result);
}
#endif

#ifndef ORLIX_TCTI_PROOF_INGESTION_HOST_TEST
static orlix_tcti_proof_u64 hash_bytes(orlix_tcti_proof_u64 value,
				       const void *bytes, size_t size)
{
	const unsigned char *cursor = bytes;
	size_t index;

	for (index = 0; index < size; index++) {
		value ^= cursor[index];
		value *= ORLIX_TCTI_PROOF_U64_C(1099511628211);
	}
	return value;
}
#endif

struct orlix_tcti_target_proof_ingestion_ledger *
orlix_tcti_target_proof_ingestion_ledger_create(size_t capacity)
{
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;

	if (!capacity ||
	    capacity > ORLIX_TCTI_TARGET_PROOF_LEDGER_MAX_RECORDS ||
	    capacity > SIZE_MAX / sizeof(*ledger->slots))
		return NULL;
	ledger = proof_alloc(sizeof(*ledger));
	if (!ledger)
		return NULL;
	ledger->slots = proof_alloc(capacity * sizeof(*ledger->slots));
	if (!ledger->slots) {
		proof_free(ledger);
		return NULL;
	}
	ledger->capacity = capacity;
	return ledger;
}

void orlix_tcti_target_proof_ingestion_ledger_destroy(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger)
{
	if (!ledger)
		return;
	proof_free(ledger->slots);
	proof_free(ledger);
}

int orlix_tcti_target_proof_ingestion_summary(
	const struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_target_proof_ingestion_summary *summary)
{
	if (!ledger || !summary)
		return -1;
	summary->accepted_records = ledger->count;
	summary->native_passed = ledger->native_passed;
	summary->kselftest_passed = ledger->kselftest_passed;
	summary->rejected = ledger->rejected;
	return 0;
}

#ifdef ORLIX_TCTI_PROOF_INGESTION_HOST_TEST
void orlix_tcti_target_native_result_record_destroy(
	struct orlix_tcti_target_native_result_record *record)
{
	if (!record)
		return;
	record->magic = 0;
	proof_free(record);
}
#endif

static void reject(struct orlix_tcti_target_proof_ingestion_ledger *ledger,
		   enum orlix_tcti_target_proof_ingestion_error value,
		   enum orlix_tcti_target_proof_ingestion_error *error)
{
	if (ledger)
		ledger->rejected++;
	if (error)
		*error = value;
}

static int ledger_insert(struct orlix_tcti_target_proof_ingestion_ledger *ledger,
			 orlix_tcti_proof_u64 identity, const void *record,
			 enum orlix_tcti_target_proof_ingestion_error *error)
{
	size_t index;

	if (!ledger || !ledger->slots || !ledger->capacity || !identity || !record) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	for (index = 0; index < ledger->count; index++)
		if (ledger->slots[index].identity == identity ||
		    ledger->slots[index].record == record) {
			reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_REPLAY, error);
			return -1;
		}
	if (ledger->count == ledger->capacity) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	ledger->slots[ledger->count].identity = identity;
	ledger->slots[ledger->count].record = record;
	ledger->count++;
	return 0;
}

static const struct orlix_tcti_target_proof_registry_entry *find_entry(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	const char *id)
{
	const struct orlix_tcti_target_proof_registry_entry *found = NULL;
	size_t index;

	if (!entries || empty(id))
		return NULL;
	for (index = 0; index < count; index++)
		if (!empty(entries[index].id) && !strcmp(entries[index].id, id)) {
			if (found)
				return NULL;
			found = &entries[index];
		}
	return found;
}

static const struct orlix_tcti_target_proof_binding *find_binding(
	const struct orlix_tcti_target_proof_registry_entry *entry,
	const struct orlix_tcti_target_native_result_record *record)
{
	const struct orlix_tcti_target_proof_binding *found = NULL;
	size_t index;

	for (index = 0; index < entry->binding_count; index++) {
		const struct orlix_tcti_target_proof_binding *binding = &entry->bindings[index];

		if (binding->source_ordinal != record->source_ordinal)
			continue;
		if (found)
			return NULL;
		found = binding;
	}
	return found;
}

static int case_index(const struct orlix_tcti_target_proof_registry_entry *entry,
		      const char *name)
{
	size_t index;

	for (index = 0; index < entry->kunit_case_count; index++)
		if (!strcmp(entry->kunit_cases[index].name, name))
			return (int)index;
	return -1;
}

static bool selector_complete(
	const struct orlix_tcti_target_native_ingestion_selector *selector)
{
	return selector && !empty(selector->proof_id) &&
	       !empty(selector->condition_tcnd_hex) &&
	       !empty(selector->kunit_source) &&
	       !empty(selector->kunit_source_sha256) &&
	       !empty(selector->kunit_build_source) &&
	       !empty(selector->kunit_build_source_sha256) &&
	       !empty(selector->kunit_suite) && !empty(selector->kunit_case) &&
	       !empty(selector->executing_kernel_identity);
}

static bool native_record_consume(
	struct orlix_tcti_target_native_result_record *record)
{
#ifdef __KERNEL__
	return atomic_cmpxchg(&record->consumed, 0, 1) == 0;
#else
	bool expected = false;

	return atomic_compare_exchange_strong(&record->consumed, &expected, true);
#endif
}

int orlix_tcti_target_proof_ingest_native(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_target_native_result_record *record,
	const struct orlix_tcti_target_native_ingestion_selector *selector,
	enum orlix_tcti_target_proof_ingestion_error *error)
{
	struct orlix_tcti_target_kunit_provenance_identity provenance;
	const struct orlix_tcti_target_proof_registry_entry *entries;
	const struct orlix_tcti_target_proof_registry_entry *entry;
	const struct orlix_tcti_target_proof_binding *binding;
	orlix_tcti_proof_u32 obligation;
	size_t entry_count;
	int selected_case;
	const struct orlix_tcti_target_instruction_artifact *artifact;

	if (error)
		*error = ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID;
	if (!record || record->magic != ORLIX_TCTI_TARGET_NATIVE_RECORD_MAGIC ||
	    !selector_complete(selector)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	entries = orlix_tcti_target_proof_registry_entries(&entry_count);
	if (!entries || !entry_count) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	artifact = orlix_tcti_target_instruction_artifact_canonical();
	if (!artifact ||
	    strcmp(record->artifact_architecture, artifact->architecture) ||
	    strcmp(record->artifact_build, artifact->build) ||
	    strcmp(record->artifact_reference, artifact->reference) ||
	    strcmp(record->artifact_schema, artifact->schema) ||
	    strcmp(record->artifact_source_sha256, artifact->source_sha256)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_STALE_SOURCE, error);
		return -1;
	}
	if (!record->production_resume || !record->source_bound || !record->match ||
	    record->resume_count != 1) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_PATH_MISMATCH, error);
		return -1;
	}
	if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_RESULT)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC;
	else if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_GPR)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS;
	else if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_DECODE)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE;
	else if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_LEGAL_ENCODINGS)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS;
	else if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_REJECTED_ENCODINGS)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS;
	else if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_REGISTERS)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS;
	else if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_PC)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC;
	else if (record->kind == ORLIX_TCTI_TARGET_NATIVE_RESULT_FLAGS)
		obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS;
	else {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_NON_PRODUCTION, error);
		return -1;
	}
	entry = find_entry(entries, entry_count, selector->proof_id);
	if (!entry) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_UNKNOWN, error);
		return -1;
	}
	binding = find_binding(entry, record);
	if (!binding || strcmp(binding->leaf_name, record->leaf_name) ||
	    strcmp(binding->mnemonic, record->mnemonic) ||
	    binding->encoding_mask != record->encoding_mask ||
	    binding->encoding_pattern != record->encoding_pattern) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_STALE_SOURCE, error);
		return -1;
	}
	if (record->kind ==
	    ORLIX_TCTI_TARGET_NATIVE_RESULT_REJECTED_ENCODINGS) {
		orlix_tcti_proof_u32 mismatch =
			(record->entry_instruction ^ binding->encoding_pattern) &
			binding->encoding_mask;

		if (!mismatch || (mismatch & (mismatch - 1U))) {
			reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_STALE_SOURCE,
			       error);
			return -1;
		}
	} else if ((record->entry_instruction & binding->encoding_mask) !=
		   binding->encoding_pattern) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_STALE_SOURCE, error);
		return -1;
	}
	if (strcmp(entry->operation_id, record->operation_id)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_FAMILY_MISMATCH, error);
		return -1;
	}
	if (entry->classification_mask != selector->classification_mask) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_CLASSIFICATION_MISMATCH, error);
		return -1;
	}
	if (empty(selector->condition_tcnd_hex) ||
	    strcmp(binding->condition_tcnd_hex, selector->condition_tcnd_hex)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_CONDITION_MISMATCH, error);
		return -1;
	}
	if (empty(selector->executing_kernel_identity) ||
	    strcmp(record->executing_kernel_identity,
		   selector->executing_kernel_identity)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_BUILD_MISMATCH, error);
		return -1;
	}
	if (strcmp(record->implementation_owner, "orlix_tcti_resume_user") ||
	    strcmp(record->decoder_owner, "orlix_tcti_decode_aarch64") ||
	    strcmp(record->lowering_owner,
		   "orlix_tcti_execute_decoded_semantics")) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_PATH_MISMATCH, error);
		return -1;
	}
	selected_case = case_index(entry, selector->kunit_case);
	if (selected_case < 0 || selected_case >= 64 ||
	    !(binding->kunit_case_mask &
	      (ORLIX_TCTI_PROOF_U64_C(1) << selected_case))) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_CASE_MISMATCH, error);
		return -1;
	}
	if (!(entry->obligations & obligation) ||
	    !(entry->unproved_obligations & obligation) ||
	    !(entry->kunit_cases[selected_case].obligations & obligation)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_OBLIGATION_MISMATCH, error);
		return -1;
	}
	if (orlix_tcti_target_kunit_provenance_identity(entry,
		selector->kunit_case, &provenance) ||
	    strcmp(selector->kunit_source, provenance.source) ||
	    strcmp(selector->kunit_source_sha256, provenance.source_sha256) ||
	    strcmp(selector->kunit_build_source, provenance.build_source) ||
	    strcmp(selector->kunit_build_source_sha256,
		   provenance.build_source_sha256) ||
	    strcmp(selector->kunit_suite, provenance.suite)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_SUITE_MISMATCH, error);
		return -1;
	}
	if (!ledger || !ledger->slots || !ledger->capacity ||
	    ledger->count == ledger->capacity) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	if (!native_record_consume(record)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_REPLAY, error);
		return -1;
	}
	if (ledger_insert(ledger, record->identity, record, error))
		return -1;
	ledger->native_passed++;
	if (error)
		*error = ORLIX_TCTI_TARGET_PROOF_INGEST_OK;
	return 0;
}

static orlix_tcti_proof_u64 kselftest_identity(
	const struct orlix_tcti_target_kselftest_result *result)
{
	orlix_tcti_proof_u64 value = ORLIX_TCTI_PROOF_U64_C(1469598103934665603);

	value = hash_bytes(value, result->identity.source,
			   strlen(result->identity.source));
	value = hash_bytes(value, result->identity.source_sha256,
			   strlen(result->identity.source_sha256));
	value = hash_bytes(value, result->identity.program,
			   strlen(result->identity.program));
	value = hash_bytes(value, result->identity.case_name,
			   strlen(result->identity.case_name));
	value = hash_bytes(value, result->executing_kernel_identity,
			   strlen(result->executing_kernel_identity));
	return value ? value : 1U;
}

int orlix_tcti_target_proof_ingest_kselftest(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	const struct orlix_tcti_target_linux_proof_disposition_row *requested_row,
	const struct orlix_tcti_target_kselftest_result *result,
	const char *executing_kernel_identity,
	enum orlix_tcti_target_proof_ingestion_error *error)
{
	const struct orlix_tcti_target_linux_proof_disposition_row *rows;
	const struct orlix_tcti_target_linux_proof_disposition_row *row = NULL;
	size_t row_count;
	size_t row_index;
	size_t index;
	bool found = false;

	if (!requested_row || !result || empty(executing_kernel_identity) ||
	    empty(result->executing_kernel_identity)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	if (result->state != ORLIX_TCTI_TARGET_KSELFTEST_PASSED) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_RESULT_NOT_PASSED, error);
		return -1;
	}
	if (strcmp(executing_kernel_identity, result->executing_kernel_identity)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_BUILD_MISMATCH, error);
		return -1;
	}
	if (orlix_tcti_target_kselftest_provenance_validate(&result->identity)) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_STALE_SOURCE, error);
		return -1;
	}
	rows = orlix_tcti_target_linux_proof_dispositions(&row_count);
	if (!rows || !row_count) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	for (row_index = 0; row_index < row_count; row_index++)
		if (&rows[row_index] == requested_row) {
			row = &rows[row_index];
			break;
		}
	if (!row) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		return -1;
	}
	if (row->disposition == ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_NOT_APPLICABLE, error);
		return -1;
	}
	for (index = 0; index < row->kselftest_count; index++) {
		const struct orlix_tcti_target_kselftest_provenance *identity =
			&row->kselftests[index];

		if (!strcmp(identity->source, result->identity.source) &&
		    !strcmp(identity->source_sha256,
			    result->identity.source_sha256) &&
		    !strcmp(identity->build_source,
			    result->identity.build_source) &&
		    !strcmp(identity->build_source_sha256,
			    result->identity.build_source_sha256) &&
		    !strcmp(identity->program, result->identity.program) &&
		    !strcmp(identity->case_name, result->identity.case_name)) {
			found = true;
			break;
		}
	}
	if (!found) {
		reject(ledger, ORLIX_TCTI_TARGET_PROOF_INGEST_CASE_MISMATCH, error);
		return -1;
	}
	if (ledger_insert(ledger, kselftest_identity(result), result, error))
		return -1;
	ledger->kselftest_passed++;
	if (error)
		*error = ORLIX_TCTI_TARGET_PROOF_INGEST_OK;
	return 0;
}
