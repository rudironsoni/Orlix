// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <string.h>

#include "target_instruction_artifact.h"
#include "target_proof_ingestion.h"
#include "target_proof_ingestion_private.h"

#define EXPECT(value) do { \
	if (!(value)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); \
		return -1; \
	} \
} while (0)

struct native_fixture {
	struct orlix_tcti_target_proof_registry_entry entry;
	struct orlix_tcti_target_proof_binding binding;
	struct orlix_tcti_target_proof_case proof_case;
	struct orlix_tcti_target_native_result_test_input input;
	struct orlix_tcti_target_native_ingestion_selector selector;
	struct orlix_tcti_target_kunit_provenance_identity provenance;
};

static int fixture_init_for(struct native_fixture *fixture,
	orlix_tcti_proof_u32 obligation,
	enum orlix_tcti_target_native_result_kind kind)
{
	const struct orlix_tcti_target_instruction_artifact *artifact;
	const struct orlix_tcti_target_proof_registry_entry *entries;
	size_t count;
	size_t entry_index;
	size_t binding_index;
	int selected_case = -1;

	memset(fixture, 0, sizeof(*fixture));
	entries = orlix_tcti_target_proof_registry_entries(&count);
	artifact = orlix_tcti_target_instruction_artifact_canonical();
	EXPECT(entries && count && artifact);
	for (entry_index = 0; entry_index < count && selected_case < 0;
	     entry_index++)
		for (binding_index = 0;
		     binding_index < entries[entry_index].binding_count;
		     binding_index++) {
			orlix_tcti_proof_u64 mask =
				entries[entry_index].bindings[binding_index]
					.kunit_case_mask;
			unsigned int index;

			for (index = 0;
			     index < entries[entry_index].kunit_case_count && index < 64;
			     index++)
			if ((mask & (ORLIX_TCTI_PROOF_U64_C(1) << index)) &&
			    (entries[entry_index].unproved_obligations & obligation) &&
			    (entries[entry_index].kunit_cases[index].obligations &
			     obligation)) {
					selected_case = (int)index;
					break;
				}
			if (selected_case >= 0)
				break;
		}
	EXPECT(selected_case >= 0);
	fixture->entry = entries[entry_index - 1U];
	fixture->binding = fixture->entry.bindings[binding_index];
	fixture->proof_case = fixture->entry.kunit_cases[selected_case];
	EXPECT(!orlix_tcti_target_kunit_provenance_identity(
		&fixture->entry, fixture->proof_case.name, &fixture->provenance));
	fixture->input.source_ordinal = fixture->binding.source_ordinal;
	fixture->input.leaf_name = fixture->binding.leaf_name;
	fixture->input.mnemonic = fixture->binding.mnemonic;
	fixture->input.operation_id = fixture->entry.operation_id;
	fixture->input.encoding_mask = fixture->binding.encoding_mask;
	fixture->input.encoding_pattern = fixture->binding.encoding_pattern;
	fixture->input.entry_instruction = fixture->binding.encoding_pattern;
	fixture->input.kind = kind;
	fixture->input.artifact_architecture = artifact->architecture;
	fixture->input.artifact_build = artifact->build;
	fixture->input.artifact_reference = artifact->reference;
	fixture->input.artifact_schema = artifact->schema;
	fixture->input.artifact_source_sha256 = artifact->source_sha256;
	fixture->input.executing_kernel_identity = "host-fixture-build";
	fixture->input.implementation_owner = "orlix_tcti_resume_user";
	fixture->input.decoder_owner = "orlix_tcti_decode_aarch64";
	fixture->input.lowering_owner = "orlix_tcti_execute_decoded_semantics";
	fixture->input.production_resume = true;
	fixture->input.source_bound = true;
	fixture->input.match = true;
	fixture->input.resume_count = 1;
	fixture->selector.proof_id = fixture->entry.id;
	fixture->selector.classification_mask = fixture->entry.classification_mask;
	fixture->selector.condition_tcnd_hex = fixture->binding.condition_tcnd_hex;
	fixture->selector.kunit_source = fixture->provenance.source;
	fixture->selector.kunit_source_sha256 = fixture->provenance.source_sha256;
	fixture->selector.kunit_build_source = fixture->provenance.build_source;
	fixture->selector.kunit_build_source_sha256 =
		fixture->provenance.build_source_sha256;
	fixture->selector.kunit_suite = fixture->provenance.suite;
	fixture->selector.kunit_case = fixture->provenance.case_name;
	fixture->selector.executing_kernel_identity = "host-fixture-build";
	return 0;
}

static int fixture_init(struct native_fixture *fixture)
{
	return fixture_init_for(fixture,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
		ORLIX_TCTI_TARGET_NATIVE_RESULT_GPR);
}

static int ingest_once(const struct native_fixture *fixture,
	const struct orlix_tcti_target_native_result_test_input *input,
	const struct orlix_tcti_target_native_ingestion_selector *selector,
	enum orlix_tcti_target_proof_ingestion_error expected)
{
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_native_result_record *record;
	enum orlix_tcti_target_proof_ingestion_error error;
	int ret;

	(void)fixture;
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(2);
	EXPECT(ledger);
	record = orlix_tcti_target_native_result_record_create_for_test(input);
	EXPECT(record);
	ret = orlix_tcti_target_proof_ingest_native(
		ledger, record, selector, &error);
	if ((expected == ORLIX_TCTI_TARGET_PROOF_INGEST_OK && ret != 0) ||
	    (expected != ORLIX_TCTI_TARGET_PROOF_INGEST_OK && ret != -1))
		fprintf(stderr, "ingest expected=%d ret=%d error=%d\n",
			expected, ret, error);
	EXPECT((expected == ORLIX_TCTI_TARGET_PROOF_INGEST_OK && ret == 0) ||
	       (expected != ORLIX_TCTI_TARGET_PROOF_INGEST_OK && ret == -1));
	EXPECT(error == expected);
	orlix_tcti_target_native_result_record_destroy(record);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	return 0;
}

static int native_mutation_matrix(void)
{
	struct native_fixture fixture;
	struct orlix_tcti_target_native_result_test_input input;
	struct orlix_tcti_target_native_ingestion_selector selector;
	EXPECT(!fixture_init(&fixture));
	EXPECT(!ingest_once(&fixture, &fixture.input, &fixture.selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_OK));
	selector = fixture.selector;
	selector.proof_id = "unknown";
	EXPECT(!ingest_once(&fixture, &fixture.input, &selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_UNKNOWN));
	input = fixture.input;
	input.leaf_name = "stale";
	EXPECT(!ingest_once(&fixture, &input, &fixture.selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_STALE_SOURCE));
	input = fixture.input;
	input.operation_id = "wrong-family";
	EXPECT(!ingest_once(&fixture, &input, &fixture.selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_FAMILY_MISMATCH));
	selector = fixture.selector;
	selector.classification_mask ^= ORLIX_TCTI_TARGET_PROOF_CLASS_NON_EL0;
	EXPECT(!ingest_once(&fixture, &fixture.input, &selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_CLASSIFICATION_MISMATCH));
	selector = fixture.selector;
	selector.condition_tcnd_hex = "00";
	EXPECT(!ingest_once(&fixture, &fixture.input, &selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_CONDITION_MISMATCH));
	selector = fixture.selector;
	selector.executing_kernel_identity = "other-build";
	EXPECT(!ingest_once(&fixture, &fixture.input, &selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_BUILD_MISMATCH));
	selector = fixture.selector;
	selector.kunit_suite = "wrong-suite";
	EXPECT(!ingest_once(&fixture, &fixture.input, &selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_SUITE_MISMATCH));
	selector = fixture.selector;
	selector.kunit_case = "wrong-case";
	EXPECT(!ingest_once(&fixture, &fixture.input, &selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_CASE_MISMATCH));
	input = fixture.input;
	input.production_resume = false;
	EXPECT(!ingest_once(&fixture, &input, &fixture.selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_PATH_MISMATCH));
	input = fixture.input;
	input.kind = ORLIX_TCTI_TARGET_NATIVE_RESULT_NON_PRODUCTION;
	EXPECT(!ingest_once(&fixture, &input, &fixture.selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_NON_PRODUCTION));
	return 0;
}

static int typed_native_result_kinds_map_to_architectural_obligations(void)
{
	static const struct {
		enum orlix_tcti_target_native_result_kind kind;
		orlix_tcti_proof_u32 obligation;
	} cases[] = {
		{ ORLIX_TCTI_TARGET_NATIVE_RESULT_MEMORY,
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
		{ ORLIX_TCTI_TARGET_NATIVE_RESULT_FP_SIMD,
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
		{ ORLIX_TCTI_TARGET_NATIVE_RESULT_FAULT,
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
		{ ORLIX_TCTI_TARGET_NATIVE_RESULT_ORDERING,
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	};
	struct native_fixture fixture;
	size_t index;

	for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
		EXPECT(!fixture_init_for(&fixture, cases[index].obligation,
			cases[index].kind));
		EXPECT(!ingest_once(&fixture, &fixture.input, &fixture.selector,
			ORLIX_TCTI_TARGET_PROOF_INGEST_OK));
	}
	return 0;
}

static int canonical_registry_cannot_be_replaced_by_caller_copy(void)
{
	struct native_fixture fixture;

	EXPECT(!fixture_init(&fixture));
	fixture.entry.id = "forged-proof";
	fixture.entry.operation_id = "forged-operation";
	fixture.entry.classification_mask ^= ORLIX_TCTI_TARGET_PROOF_CLASS_NON_EL0;
	fixture.proof_case.obligations = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC;
	EXPECT(!ingest_once(&fixture, &fixture.input, &fixture.selector,
		ORLIX_TCTI_TARGET_PROOF_INGEST_OK));
	return 0;
}

static int null_selector_fields_fail_closed(void)
{
	struct native_fixture fixture;
	struct orlix_tcti_target_native_ingestion_selector selector;

	EXPECT(!fixture_init(&fixture));
#define EXPECT_NULL_SELECTOR_FIELD(field) \
	do { \
		selector = fixture.selector; \
		selector.field = NULL; \
		EXPECT(!ingest_once(&fixture, &fixture.input, &selector, \
			ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID)); \
	} while (0)
	EXPECT_NULL_SELECTOR_FIELD(proof_id);
	EXPECT_NULL_SELECTOR_FIELD(condition_tcnd_hex);
	EXPECT_NULL_SELECTOR_FIELD(kunit_source);
	EXPECT_NULL_SELECTOR_FIELD(kunit_source_sha256);
	EXPECT_NULL_SELECTOR_FIELD(kunit_build_source);
	EXPECT_NULL_SELECTOR_FIELD(kunit_build_source_sha256);
	EXPECT_NULL_SELECTOR_FIELD(kunit_suite);
	EXPECT_NULL_SELECTOR_FIELD(kunit_case);
	EXPECT_NULL_SELECTOR_FIELD(executing_kernel_identity);
#undef EXPECT_NULL_SELECTOR_FIELD
	return 0;
}

static int replay_is_rejected(void)
{
	struct native_fixture fixture;
	struct orlix_tcti_target_proof_ingestion_ledger *first_ledger;
	struct orlix_tcti_target_proof_ingestion_ledger *second_ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;
	struct orlix_tcti_target_native_result_record *record;
	enum orlix_tcti_target_proof_ingestion_error error;

	EXPECT(!fixture_init(&fixture));
	first_ledger = orlix_tcti_target_proof_ingestion_ledger_create(2);
	second_ledger = orlix_tcti_target_proof_ingestion_ledger_create(2);
	EXPECT(first_ledger && second_ledger);
	record = orlix_tcti_target_native_result_record_create_for_test(
		&fixture.input);
	EXPECT(record);
	EXPECT(!orlix_tcti_target_proof_ingest_native(
		first_ledger, record, &fixture.selector, &error));
	EXPECT(orlix_tcti_target_proof_ingest_native(
		second_ledger, record, &fixture.selector, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_INGEST_REPLAY);
	EXPECT(!orlix_tcti_target_proof_ingestion_summary(first_ledger, &summary));
	EXPECT(summary.native_passed == 1 && summary.accepted_records == 1);
	EXPECT(!orlix_tcti_target_proof_ingestion_summary(second_ledger, &summary));
	EXPECT(summary.native_passed == 0 && summary.accepted_records == 0 &&
	       summary.rejected == 1);
	orlix_tcti_target_native_result_record_destroy(record);
	orlix_tcti_target_proof_ingestion_ledger_destroy(first_ledger);
	orlix_tcti_target_proof_ingestion_ledger_destroy(second_ledger);
	return 0;
}

static int ledger_capacity_covers_complete_contract(void)
{
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;

	EXPECT(ORLIX_TCTI_TARGET_PROOF_LEDGER_MAX_RECORDS ==
	       (size_t)ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS * 11U);
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(
		ORLIX_TCTI_TARGET_PROOF_LEDGER_MAX_RECORDS);
	EXPECT(ledger);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	EXPECT(!orlix_tcti_target_proof_ingestion_ledger_create(
		ORLIX_TCTI_TARGET_PROOF_LEDGER_MAX_RECORDS + 1U));
	EXPECT(!orlix_tcti_target_proof_ingestion_ledger_create(SIZE_MAX));
	return 0;
}

static int kselftest_typed_results_fail_closed(void)
{
	const struct orlix_tcti_target_linux_proof_disposition_row *rows;
	const struct orlix_tcti_target_linux_proof_disposition_row *owned = NULL;
	struct orlix_tcti_target_linux_proof_disposition_row copied_row;
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;
	struct orlix_tcti_target_kselftest_result *result;
	enum orlix_tcti_target_proof_ingestion_error error;
	size_t count;
	size_t index;

	rows = orlix_tcti_target_linux_proof_dispositions(&count);
	for (index = 0; index < count; index++) {
		if (!owned && rows[index].disposition ==
				      ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED &&
		    rows[index].kselftest_count)
			owned = &rows[index];
	}
	EXPECT(owned);
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(2);
	EXPECT(ledger);
	result = orlix_tcti_target_kselftest_result_create_for_test(
		&owned->kselftests[0], ORLIX_TCTI_TARGET_KSELFTEST_FAILED,
		"kernel-build");
	EXPECT(result);
	EXPECT(orlix_tcti_target_proof_ingest_kselftest(
		ledger, owned, result, "kernel-build", &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_INGEST_RESULT_NOT_PASSED);
	orlix_tcti_target_kselftest_result_destroy_for_test(result);
	result = orlix_tcti_target_kselftest_result_create_for_test(
		&owned->kselftests[0], ORLIX_TCTI_TARGET_KSELFTEST_SKIPPED,
		"kernel-build");
	EXPECT(orlix_tcti_target_proof_ingest_kselftest(
		ledger, owned, result, "kernel-build", &error) == -1);
	orlix_tcti_target_kselftest_result_destroy_for_test(result);
	result = orlix_tcti_target_kselftest_result_create_for_test(
		&owned->kselftests[0], ORLIX_TCTI_TARGET_KSELFTEST_UNEXECUTED,
		"kernel-build");
	EXPECT(orlix_tcti_target_proof_ingest_kselftest(
		ledger, owned, result, "kernel-build", &error) == -1);
	orlix_tcti_target_kselftest_result_destroy_for_test(result);
	result = orlix_tcti_target_kselftest_result_create_for_test(
		&owned->kselftests[0], ORLIX_TCTI_TARGET_KSELFTEST_INCOMPLETE,
		"kernel-build");
	EXPECT(orlix_tcti_target_proof_ingest_kselftest(
		ledger, owned, result, "kernel-build", &error) == -1);
	orlix_tcti_target_kselftest_result_destroy_for_test(result);
	result = orlix_tcti_target_kselftest_result_create_for_test(
		&owned->kselftests[0], ORLIX_TCTI_TARGET_KSELFTEST_PASSED,
		"kernel-build");
	copied_row = *owned;
	EXPECT(orlix_tcti_target_proof_ingest_kselftest(
		ledger, &copied_row, result, "kernel-build", &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID);
	EXPECT(!orlix_tcti_target_proof_ingest_kselftest(
		ledger, owned, result, "kernel-build", &error));
	EXPECT(!orlix_tcti_target_proof_ingestion_summary(ledger, &summary));
	EXPECT(summary.kselftest_passed == 1);
	orlix_tcti_target_kselftest_result_destroy_for_test(result);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	return 0;
}

int main(void)
{
	if (native_mutation_matrix() ||
	    typed_native_result_kinds_map_to_architectural_obligations() ||
	    canonical_registry_cannot_be_replaced_by_caller_copy() ||
	    null_selector_fields_fail_closed() || replay_is_rejected() ||
	    ledger_capacity_covers_complete_contract() ||
	    kselftest_typed_results_fail_closed())
		return 1;
	puts("target proof ingestion tests passed: typed-obligations "
	     "canonical-registry null-selectors cross-ledger-replay");
	return 0;
}
