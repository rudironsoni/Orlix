/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_proof_registry.h"
#include "target_instruction_artifact.h"

#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(value) do { \
	if (!(value)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); \
		return -1; \
	} \
} while (0)

#define CONCURRENT_FIRST_USE_THREADS 16U

struct concurrent_first_use_result {
	const struct orlix_tcti_target_proof_registry_entry *entries;
	const struct orlix_tcti_target_linux_proof_disposition_row *rows;
	size_t entry_count;
	size_t row_count;
	bool stable;
};

static atomic_uint registry_waiters;
static atomic_uint row_waiters;
static atomic_bool release_registry_waiters;
static atomic_bool release_row_waiters;

static void *concurrent_first_use_worker(void *context)
{
	struct concurrent_first_use_result *result = context;
	unsigned int iteration;

	atomic_fetch_add_explicit(&registry_waiters, 1U, memory_order_release);
	while (!atomic_load_explicit(&release_registry_waiters,
				     memory_order_acquire))
		sched_yield();
	result->entries = orlix_tcti_target_proof_registry_entries(
		&result->entry_count);
	atomic_fetch_add_explicit(&row_waiters, 1U, memory_order_release);
	while (!atomic_load_explicit(&release_row_waiters, memory_order_acquire))
		sched_yield();
	result->rows = orlix_tcti_target_linux_proof_dispositions(
		&result->row_count);
	result->stable = result->entries && result->rows;
	for (iteration = 0; iteration < 128U && result->stable; iteration++) {
		size_t entry_count;
		size_t row_count;

		result->stable =
			orlix_tcti_target_proof_registry_entries(&entry_count) ==
				result->entries &&
			entry_count == result->entry_count &&
			orlix_tcti_target_linux_proof_dispositions(&row_count) ==
				result->rows &&
			row_count == result->row_count;
	}
	return NULL;
}

static int canonical_first_use_is_concurrent_and_immutable(void)
{
	pthread_t threads[CONCURRENT_FIRST_USE_THREADS];
	struct concurrent_first_use_result results[CONCURRENT_FIRST_USE_THREADS] = { 0 };
	unsigned int index;

	for (index = 0; index < CONCURRENT_FIRST_USE_THREADS; index++)
		EXPECT(!pthread_create(&threads[index], NULL,
				       concurrent_first_use_worker, &results[index]));
	while (atomic_load_explicit(&registry_waiters, memory_order_acquire) !=
	       CONCURRENT_FIRST_USE_THREADS)
		sched_yield();
	atomic_store_explicit(&release_registry_waiters, true,
			      memory_order_release);
	while (atomic_load_explicit(&row_waiters, memory_order_acquire) !=
	       CONCURRENT_FIRST_USE_THREADS)
		sched_yield();
	atomic_store_explicit(&release_row_waiters, true, memory_order_release);
	for (index = 0; index < CONCURRENT_FIRST_USE_THREADS; index++)
		EXPECT(!pthread_join(threads[index], NULL));
	for (index = 0; index < CONCURRENT_FIRST_USE_THREADS; index++) {
		EXPECT(results[index].stable);
		EXPECT(results[index].entries == results[0].entries);
		EXPECT(results[index].entry_count == results[0].entry_count);
		EXPECT(results[index].rows == results[0].rows);
		EXPECT(results[index].row_count ==
		       ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS);
	}
	return 0;
}

#define TRUE_CONDITION \
	"54434e440107000000220700000017070000000c010000000101010000000101010000000101010000000101"
#define ERET_CONDITION TRUE_CONDITION
#define LSE_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f4c5345"
#define DECODE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_decode_test.c"
#define LSE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_lse_decode_test.c"
#define CSSC_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_cssc_min_max_immediate_test.c"
#define ADD_SUB_IMMEDIATE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_add_sub_immediate_test.c"
#define ADD_SUB_IMMEDIATE_SUITE "orlix-tcti-add-sub-immediate"
#define CSSC_CONDITION \
	"54434e4401070000002e0700000017070000000c010000000101010000000101010000000101020000000d00000009464541545f43535343"
#define BASELINE \
	(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS)
#define ADD_OBLIGATIONS \
	(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define ADD_FLAGS_OBLIGATIONS \
	(ADD_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define INTEGER_CONDITIONAL_OBLIGATIONS \
	(BASELINE | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS \
	(INTEGER_CONDITIONAL_OBLIGATIONS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define LSE_OBLIGATIONS \
	(BASELINE | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING)
#define CSSC_OBLIGATIONS \
	(BASELINE | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define SCALAR_FP_CONVERT_OBLIGATIONS \
	(BASELINE | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)

static const struct orlix_tcti_target_proof_binding add_bindings[] = {
	{ "ADD_32_addsub_imm", "ADD", 0xff800000U, 0x11000000U,
	  TRUE_CONDITION, UINT64_C(0x1f), 2173U },
	{ "ADD_64_addsub_imm", "ADD", 0xff800000U, 0x91000000U,
	  TRUE_CONDITION, UINT64_C(0x1f), 2177U },
};
static const struct orlix_tcti_target_proof_case add_cases[] = {
	{ "orlix_tcti_add_sub_immediate_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_add_sub_immediate_all_legal_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_add_sub_immediate_production_path_arithmetic",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_add_sub_immediate_special_register_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_add_sub_immediate_source_mask_boundaries",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE },
};
static const struct orlix_tcti_target_proof_registry_entry add_entry = {
	"kunit:add-sub-immediate-add", "ADD_addsub_imm",
	ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, ADD_OBLIGATIONS,
	ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
	ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	add_cases, sizeof(add_cases) / sizeof(add_cases[0]), add_bindings, 2,
	NULL, ADD_OBLIGATIONS,
};

static int real_manifest_bindings_are_exact(void)
{
	enum orlix_tcti_target_proof_registry_error error;
	int result;
	struct orlix_tcti_target_proof_reference reference = {
		"kunit:add-sub-immediate-add", "ADD_32_addsub_imm", "ADD",
		"ADD_addsub_imm", 0xff800000U, 0x11000000U,
		TRUE_CONDITION, 1,
	};

	result = orlix_tcti_target_proof_registry_validate(&add_entry, 1, &error);
	if (result)
		fprintf(stderr, "registry validation error: %d\n", error);
	EXPECT(result == 0);
	EXPECT(orlix_tcti_target_proof_registry_lookup(&add_entry, 1, &reference) ==
	       ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
	reference.encoding_pattern = 0x91000000U;
	EXPECT(orlix_tcti_target_proof_registry_lookup(&add_entry, 1, &reference) ==
	       ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	reference.encoding_pattern = 0x11000000U;
	reference.condition_tcnd_hex = "00";
	EXPECT(orlix_tcti_target_proof_registry_lookup(&add_entry, 1, &reference) ==
	       ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	{
		struct orlix_tcti_target_proof_binding invalid_binding = add_bindings[0];
		struct orlix_tcti_target_proof_registry_entry invalid = add_entry;
		enum orlix_tcti_target_proof_registry_error error;

		invalid_binding.source_ordinal = 2174U;
		invalid.bindings = &invalid_binding;
		invalid.binding_count = 1;
		EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
		EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	}
	return 0;
}

static int registry_fails_closed_on_unknown_operations_and_bits(void)
{
	struct orlix_tcti_target_proof_registry_entry invalid = add_entry;
	enum orlix_tcti_target_proof_registry_error error;

	invalid.operation_id = "ADD";
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_FAMILY_REQUIREMENTS);
	invalid = add_entry;
	invalid.classification_mask |= 1U << 31;
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	invalid = add_entry;
	invalid.obligations |= 1U << 31;
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	return 0;
}

static int required_operation_maps_once(void)
{
	struct orlix_tcti_target_proof_registry_entry duplicate[] = {
		add_entry, add_entry,
	};
	enum orlix_tcti_target_proof_registry_error error;

	duplicate[1].id = "kunit:add-sub-immediate-duplicate";
	EXPECT(orlix_tcti_target_proof_registry_validate(duplicate, 2, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	return 0;
}

static int explicit_binding_set_is_mandatory_and_unique(void)
{
	struct orlix_tcti_target_proof_registry_entry invalid = add_entry;
	struct orlix_tcti_target_proof_binding duplicate[] = {
		add_bindings[0], add_bindings[0],
	};
	enum orlix_tcti_target_proof_registry_error error;

	invalid.bindings = NULL;
	invalid.binding_count = 0;
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	invalid = add_entry;
	invalid.bindings = duplicate;
	invalid.binding_count = 2;
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	return 0;
}

static int cross_operation_multi_binding_is_rejected(void)
{
	static const struct orlix_tcti_target_proof_binding bindings[] = {
		{ "ADD_32_addsub_imm", "ADD", 0xff800000U, 0x11000000U,
		  TRUE_CONDITION, UINT64_C(0x1f), 2173U },
		{ "ERET_64E_branch_reg", "ERET", 0xffffffffU, 0xd69f03e0U,
		  ERET_CONDITION, UINT64_C(0x1f), 2299U },
	};
	struct orlix_tcti_target_proof_registry_entry invalid = add_entry;
	enum orlix_tcti_target_proof_registry_error error;

	invalid.bindings = bindings;
	invalid.binding_count = sizeof(bindings) / sizeof(bindings[0]);
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	return 0;
}

static int lse_cannot_omit_atomicity(void)
{
	static const struct orlix_tcti_target_proof_binding bindings[] = {
		{ "LDADD_32_memop", "LDADD", 0xffe0fc00U, 0xb8200000U,
		  LSE_CONDITION, UINT64_C(1), 3099U },
	};
	static const struct orlix_tcti_target_proof_case cases[] = {
		{ "orlix_tcti_lse_execute_rmw_matrix",
		  LSE_OBLIGATIONS & ~ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	};
	const struct orlix_tcti_target_proof_registry_entry entry = {
		"kunit:lse-ldadd", "LDADD",
		ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
		LSE_OBLIGATIONS & ~ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY,
		ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
		LSE_SOURCE, "orlix-tcti-lse-decode", cases, 1, bindings, 1,
		NULL,
		LSE_OBLIGATIONS & ~ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY,
	};
	enum orlix_tcti_target_proof_registry_error error;

	EXPECT(orlix_tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS);
	return 0;
}

static int kunit_provenance_must_be_registered(void)
{
	static const struct orlix_tcti_target_proof_case fabricated[] = {
		{ "case", ADD_OBLIGATIONS },
	};
	struct orlix_tcti_target_proof_registry_entry invalid = add_entry;
	struct orlix_tcti_target_proof_binding binding = add_bindings[0];
	enum orlix_tcti_target_proof_registry_error error;

	binding.kunit_case_mask = UINT64_C(1);
	invalid.kunit_cases = fabricated;
	invalid.kunit_case_count = 1;
	invalid.bindings = &binding;
	invalid.binding_count = 1;
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	invalid = add_entry;
	invalid.kunit_suite = "FEAT_FIRST";
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	invalid = add_entry;
	invalid.kunit_source = "/private/tmp/fabricated-kunit-source.c";
	EXPECT(orlix_tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	return 0;
}

static int registered_case_cannot_overclaim_obligations(void)
{
	static const struct orlix_tcti_target_proof_binding bindings[] = {
		{ "LDADD_32_memop", "LDADD", 0xffe0fc00U, 0xb8200000U,
		  LSE_CONDITION, UINT64_C(1), 3099U },
	};
	static const struct orlix_tcti_target_proof_case cases[] = {
		{ "orlix_tcti_gadget_executes_complete_add_sub_immediate_family",
		  LSE_OBLIGATIONS },
	};
	const struct orlix_tcti_target_proof_registry_entry entry = {
		"kunit:lse-overclaim", "LDADD",
		ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, LSE_OBLIGATIONS,
		ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
		DECODE_SOURCE, "orlix-tcti-decode", cases, 1, bindings, 1,
		NULL, LSE_OBLIGATIONS,
	};
	enum orlix_tcti_target_proof_registry_error error;

	EXPECT(orlix_tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	return 0;
}

static int typed_kselftest_provenance_is_source_and_build_bound(void)
{
	struct orlix_tcti_target_kselftest_provenance provenance = {
		"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/orlix_tcti_lse_atomic_probe.c",
		"dfe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f",
		"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/Makefile",
		"4383acf5e999267d59c0bf3035eb024e0e43180da9597ea8c15ee14d45a802fd",
		"orlix_tcti_lse_atomic_probe", "main",
	};

	EXPECT(orlix_tcti_target_kselftest_provenance_validate(&provenance) == 0);
	provenance.source_sha256 =
		"0fe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f";
	EXPECT(orlix_tcti_target_kselftest_provenance_validate(&provenance) == -1);
	provenance.source_sha256 =
		"dfe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f";
	provenance.build_source_sha256 =
		"0d858a5791a52f39bd0b5616b57c8003eda2c95cda61fc2091b25d0dbfa81a9c";
	EXPECT(orlix_tcti_target_kselftest_provenance_validate(&provenance) == -1);
	return 0;
}

static int privileged_profiles_and_count_caps_fail_closed(void)
{
	struct orlix_tcti_target_proof_registry_entry excessive = add_entry;
	enum orlix_tcti_target_proof_registry_error error;
	uint32_t requirements;

	EXPECT(orlix_tcti_target_proof_operation_requirements("SVC", 2,
						       &requirements) == 0);
	EXPECT((requirements & ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS) != 0);
	EXPECT((requirements & ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS) != 0);
	EXPECT((requirements & ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY) != 0);
	EXPECT(orlix_tcti_target_proof_operation_requirements("UDF_perm_undef", 3,
						       &requirements) == 0);
	EXPECT((requirements & ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS) != 0);
	EXPECT((requirements & ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS) != 0);
	EXPECT(orlix_tcti_target_proof_operation_requirements("not-mapped", 1,
						       &requirements) == -1);
	EXPECT(orlix_tcti_target_proof_registry_validate(&add_entry, 4351,
						  &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	excessive.binding_count = 4351;
	EXPECT(orlix_tcti_target_proof_registry_validate(&excessive, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	excessive = add_entry;
	excessive.kunit_case_count = 65;
	EXPECT(orlix_tcti_target_proof_registry_validate(&excessive, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	EXPECT(orlix_tcti_target_proof_source_size_allowed(2U * 1024U * 1024U));
	EXPECT(!orlix_tcti_target_proof_source_size_allowed(
		2U * 1024U * 1024U + 1U));
	EXPECT(!orlix_tcti_target_proof_source_size_allowed(UINT64_MAX));
	return 0;
}

static int logical_shifted_register_registry_is_source_bound(void)
{
	static const char *const operations[] = {
		"AND_log_shift", "BIC_log_shift", "ORR_log_shift",
		"ORN_log_shift", "EOR_log_shift", "EON",
		"ANDS_log_shift", "BICS",
	};
	const struct orlix_tcti_target_proof_registry_entry *entries;
	enum orlix_tcti_target_proof_registry_error error;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(count >= sizeof(operations) / sizeof(operations[0]));
	if (orlix_tcti_target_proof_registry_validate(entries, count, &error)) {
		fprintf(stderr, "%s:%d: registry validation error %u\n",
			__FILE__, __LINE__, (unsigned int)error);
		return -1;
	}
	EXPECT(orlix_tcti_target_proof_registry_source_bound_projection_validate(
		       entries, count, &error) == 0);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
	for (index = 0; index < sizeof(operations) / sizeof(operations[0]); index++) {
		uint32_t requirements;

		EXPECT(!strcmp(entries[index].operation_id, operations[index]));
		EXPECT(entries[index].classification_mask ==
		       ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0);
		EXPECT(entries[index].linux_interface ==
		       ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE);
		EXPECT(entries[index].binding_count == 2);
		EXPECT(orlix_tcti_target_proof_operation_requirements(
			       operations[index], 1, &requirements) == 0);
		EXPECT(entries[index].obligations == requirements);
	}
	return 0;
}

static int cssc_min_max_immediate_registry_is_source_bound(void)
{
	static const struct {
		const char *proof_id;
		const char *operation;
		const char *leaf;
		const char *mnemonic;
		uint32_t pattern;
	} bindings[] = {
		{ "kunit:cssc-min-max-immediate-smax", "SMAX_imm",
		  "SMAX_32_minmax_imm", "SMAX", 0x11c00000U },
		{ "kunit:cssc-min-max-immediate-umax", "UMAX_imm",
		  "UMAX_32U_minmax_imm", "UMAX", 0x11c40000U },
		{ "kunit:cssc-min-max-immediate-smin", "SMIN_imm",
		  "SMIN_32_minmax_imm", "SMIN", 0x11c80000U },
		{ "kunit:cssc-min-max-immediate-umin", "UMIN_imm",
		  "UMIN_32U_minmax_imm", "UMIN", 0x11cc0000U },
		{ "kunit:cssc-min-max-immediate-smax", "SMAX_imm",
		  "SMAX_64_minmax_imm", "SMAX", 0x91c00000U },
		{ "kunit:cssc-min-max-immediate-umax", "UMAX_imm",
		  "UMAX_64U_minmax_imm", "UMAX", 0x91c40000U },
		{ "kunit:cssc-min-max-immediate-smin", "SMIN_imm",
		  "SMIN_64_minmax_imm", "SMIN", 0x91c80000U },
		{ "kunit:cssc-min-max-immediate-umin", "UMIN_imm",
		  "UMIN_64U_minmax_imm", "UMIN", 0x91cc0000U },
		{ "kunit:cssc-data-processing-smax", "SMAX_reg",
		  "SMAX_32_dp_2src", "SMAX", 0x1ac06000U },
		{ "kunit:cssc-data-processing-umax", "UMAX_reg",
		  "UMAX_32_dp_2src", "UMAX", 0x1ac06400U },
		{ "kunit:cssc-data-processing-smin", "SMIN_reg",
		  "SMIN_32_dp_2src", "SMIN", 0x1ac06800U },
		{ "kunit:cssc-data-processing-umin", "UMIN_reg",
		  "UMIN_32_dp_2src", "UMIN", 0x1ac06c00U },
		{ "kunit:cssc-data-processing-smax", "SMAX_reg",
		  "SMAX_64_dp_2src", "SMAX", 0x9ac06000U },
		{ "kunit:cssc-data-processing-umax", "UMAX_reg",
		  "UMAX_64_dp_2src", "UMAX", 0x9ac06400U },
		{ "kunit:cssc-data-processing-smin", "SMIN_reg",
		  "SMIN_64_dp_2src", "SMIN", 0x9ac06800U },
		{ "kunit:cssc-data-processing-umin", "UMIN_reg",
		  "UMIN_64_dp_2src", "UMIN", 0x9ac06c00U },
		{ "kunit:cssc-data-processing-ctz", "CTZ",
		  "CTZ_32_dp_1src", "CTZ", 0x5ac01800U },
		{ "kunit:cssc-data-processing-cnt", "CNT",
		  "CNT_32_dp_1src", "CNT", 0x5ac01c00U },
		{ "kunit:cssc-data-processing-abs", "ABS",
		  "ABS_32_dp_1src", "ABS", 0x5ac02000U },
		{ "kunit:cssc-data-processing-ctz", "CTZ",
		  "CTZ_64_dp_1src", "CTZ", 0xdac01800U },
		{ "kunit:cssc-data-processing-cnt", "CNT",
		  "CNT_64_dp_1src", "CNT", 0xdac01c00U },
		{ "kunit:cssc-data-processing-abs", "ABS",
		  "ABS_64_dp_1src", "ABS", 0xdac02000U },
	};
	const struct orlix_tcti_target_proof_registry_entry *entries;
	enum orlix_tcti_target_proof_registry_error error;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(count >= sizeof(bindings) / sizeof(bindings[0]) / 2);
	EXPECT(orlix_tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
	for (index = 0; index < sizeof(bindings) / sizeof(bindings[0]); index++) {
		struct orlix_tcti_target_proof_reference reference = {
			bindings[index].proof_id, bindings[index].leaf,
			bindings[index].mnemonic, bindings[index].operation,
			strstr(bindings[index].leaf, "_minmax_imm") ?
				0xfffc0000U :
				(strstr(bindings[index].leaf, "_dp_1src") ?
				 0xfffffc00U : 0xffe0fc00U),
			bindings[index].pattern, CSSC_CONDITION, 1,
		};
		uint32_t requirements;

		EXPECT(orlix_tcti_target_proof_operation_requirements(
			       bindings[index].operation, 1, &requirements) == 0);
		EXPECT(requirements == CSSC_OBLIGATIONS);
		EXPECT(orlix_tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
		reference.encoding_pattern ^= 0x40000U;
		EXPECT(orlix_tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	}
	return 0;
}

static int add_sub_immediate_registry_is_source_bound(void)
{
	static const struct {
		const char *proof_id;
		const char *operation;
		const char *leaf;
		const char *mnemonic;
		uint32_t pattern;
		uint32_t obligations;
	} bindings[] = {
		{ "kunit:add-sub-immediate-add", "ADD_addsub_imm",
		  "ADD_32_addsub_imm", "ADD", 0x11000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-adds", "ADDS_addsub_imm",
		  "ADDS_32S_addsub_imm", "ADDS", 0x31000000U,
		  ADD_FLAGS_OBLIGATIONS },
		{ "kunit:add-sub-immediate-sub", "SUB_addsub_imm",
		  "SUB_32_addsub_imm", "SUB", 0x51000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-subs", "SUBS_addsub_imm",
		  "SUBS_32S_addsub_imm", "SUBS", 0x71000000U,
		  ADD_FLAGS_OBLIGATIONS },
		{ "kunit:add-sub-immediate-add", "ADD_addsub_imm",
		  "ADD_64_addsub_imm", "ADD", 0x91000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-adds", "ADDS_addsub_imm",
		  "ADDS_64S_addsub_imm", "ADDS", 0xb1000000U,
		  ADD_FLAGS_OBLIGATIONS },
		{ "kunit:add-sub-immediate-sub", "SUB_addsub_imm",
		  "SUB_64_addsub_imm", "SUB", 0xd1000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-subs", "SUBS_addsub_imm",
		  "SUBS_64S_addsub_imm", "SUBS", 0xf1000000U,
		  ADD_FLAGS_OBLIGATIONS },
	};
	const struct orlix_tcti_target_proof_registry_entry *entries;
	enum orlix_tcti_target_proof_registry_error error;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(orlix_tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
	for (index = 0; index < sizeof(bindings) / sizeof(bindings[0]); index++) {
		struct orlix_tcti_target_proof_reference reference = {
			bindings[index].proof_id, bindings[index].leaf,
			bindings[index].mnemonic, bindings[index].operation,
			0xff800000U, bindings[index].pattern,
			TRUE_CONDITION, 1,
		};
		uint32_t requirements;

		EXPECT(orlix_tcti_target_proof_operation_requirements(
			       bindings[index].operation, 1, &requirements) == 0);
		EXPECT(requirements == bindings[index].obligations);
		EXPECT(orlix_tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
		reference.encoding_pattern ^= 0x1000000U;
		EXPECT(orlix_tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	}
	return 0;
}

static int lse_registry_binds_180_leaves_without_claiming_completion(void)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	enum orlix_tcti_target_proof_registry_error error;
	size_t binding_count = 0;
	size_t entry_count = 0;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(orlix_tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(orlix_tcti_target_proof_registry_source_bound_projection_validate(
		       entries, count, &error) == 0);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&entries[index];

		if (strncmp(entry->id, "kunit:lse-base-", 15) &&
		    strncmp(entry->id, "kunit:lse128-", 13))
			continue;
		EXPECT(entry->obligations == LSE_OBLIGATIONS);
		EXPECT(entry->unproved_obligations == entry->obligations);
		EXPECT(entry->linux_interface ==
		       ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE);
		entry_count++;
		binding_count += entry->binding_count;
	}
	EXPECT(entry_count == 34);
	EXPECT(binding_count == 180);
	return 0;
}

static int lse_registry_cannot_clear_unproved_duties_statically(void)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	struct orlix_tcti_target_proof_registry_entry entry;
	enum orlix_tcti_target_proof_registry_error error;
	size_t count;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	entry = entries[count - 1];
	EXPECT(entry.unproved_obligations != 0);
	entry.unproved_obligations = 0;
	EXPECT(orlix_tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS);
	return 0;
}

static int scalar_source_bindings_are_complete_and_fail_closed(void)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	struct orlix_tcti_target_proof_registry_entry copied[256];
	struct orlix_tcti_target_proof_binding duplicate[2];
	struct orlix_tcti_target_proof_registry_entry entry;
	enum orlix_tcti_target_proof_registry_error error;
	size_t binding_count = 0;
	size_t entry_count = 0;
	size_t count;
	size_t index;
	size_t scalar_index = 0;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(count <= sizeof(copied) / sizeof(copied[0]));
	EXPECT(orlix_tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(orlix_tcti_target_proof_registry_source_bound_projection_validate(
		       entries, count, &error) == 0);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_target_proof_registry_entry *candidate =
			&entries[index];

		if (strncmp(candidate->id, "kunit:logical-immediate-", 24) &&
		    strncmp(candidate->id, "kunit:move-wide-", 16) &&
		    strncmp(candidate->id, "kunit:scalar-bitops-", 20) &&
		    strncmp(candidate->id, "kunit:add-sub-register-", 23))
			continue;
		EXPECT(candidate->unproved_obligations == candidate->obligations);
		EXPECT(candidate->linux_interface ==
		       ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE);
		if (!strcmp(candidate->id, "kunit:logical-immediate-and"))
			scalar_index = index;
		entry_count++;
		binding_count += candidate->binding_count;
	}
	EXPECT(entry_count == 25);
	EXPECT(binding_count == 49);

	/* A missing binding must leave its source-bound proof row unmatched. */
	memcpy(copied, entries, count * sizeof(copied[0]));
	EXPECT(copied[scalar_index].binding_count == 2);
	copied[scalar_index].binding_count--;
	EXPECT(orlix_tcti_target_proof_registry_source_bound_projection_validate(
		       copied, count, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);

	/* A duplicate source tuple and a changed pinned tuple both fail locally. */
	entry = entries[scalar_index];
	memcpy(duplicate, entry.bindings, sizeof(duplicate));
	duplicate[1] = duplicate[0];
	entry.bindings = duplicate;
	EXPECT(orlix_tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	duplicate[1] = entries[scalar_index].bindings[1];
	duplicate[0].encoding_pattern ^= 0x1000000U;
	EXPECT(orlix_tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	return 0;
}

static int exclusive_registry_binds_exact_baseline_leaves(void)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	enum orlix_tcti_target_proof_registry_error error;
	size_t binding_count = 0;
	size_t entry_count = 0;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(orlix_tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(orlix_tcti_target_proof_registry_source_bound_projection_validate(
		       entries, count, &error) == 0);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&entries[index];

		if (strncmp(entry->id, "kunit:exclusive-", 16))
			continue;
		EXPECT(entry->classification_mask ==
		       ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0);
		EXPECT(entry->obligations == LSE_OBLIGATIONS);
		EXPECT(entry->unproved_obligations == entry->obligations);
		EXPECT(entry->linux_interface ==
		       ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE);
		entry_count++;
		binding_count += entry->binding_count;
	}
	EXPECT(entry_count == 16);
	EXPECT(binding_count == 24);
	return 0;
}

static int branch_control_registry_binds_exact_source_rows(void)
{
	static const struct {
		uint32_t ordinal;
		const char *proof_id;
		const char *leaf_name;
		const char *mnemonic;
		const char *operation_id;
	} expected[] = {
		{ 2227U, "kunit:branch-control-svc", "SVC_EX_exception",
		  "SVC", "SVC" },
		{ 2230U, "kunit:branch-control-brk", "BRK_EX_exception",
		  "BRK", "BRK" },
		{ 2231U, "kunit:branch-control-hlt", "HLT_EX_exception",
		  "HLT", "HLT" },
		{ 2211U, "kunit:branch-control-b-cond", "B_only_condbranch",
		  "B", "B_cond" },
		{ 2288U, "kunit:branch-control-br", "BR_64_branch_reg",
		  "BR", "BR" },
		{ 2291U, "kunit:branch-control-blr", "BLR_64_branch_reg",
		  "BLR", "BLR" },
		{ 2294U, "kunit:branch-control-ret", "RET_64R_branch_reg",
		  "RET", "RET" },
		{ 2312U, "kunit:branch-control-b-uncond",
		  "B_only_branch_imm", "B", "B_uncond" },
		{ 2313U, "kunit:branch-control-bl", "BL_only_branch_imm",
		  "BL", "BL" },
		{ 2314U, "kunit:branch-control-cbz", "CBZ_32_compbranch",
		  "CBZ", "CBZ" },
		{ 2315U, "kunit:branch-control-cbnz", "CBNZ_32_compbranch",
		  "CBNZ", "CBNZ" },
		{ 2316U, "kunit:branch-control-cbz", "CBZ_64_compbranch",
		  "CBZ", "CBZ" },
		{ 2317U, "kunit:branch-control-cbnz", "CBNZ_64_compbranch",
		  "CBNZ", "CBNZ" },
		{ 2342U, "kunit:branch-control-tbz", "TBZ_only_testbranch",
		  "TBZ", "TBZ" },
		{ 2343U, "kunit:branch-control-tbnz", "TBNZ_only_testbranch",
		  "TBNZ", "TBNZ" },
	};
	const struct orlix_tcti_target_proof_registry_entry *entries;
	enum orlix_tcti_target_proof_registry_error error;
	size_t binding_count = 0;
	size_t entry_count = 0;
	size_t count;
	size_t index;
	size_t expected_index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(orlix_tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(orlix_tcti_target_proof_registry_source_bound_projection_validate(
		       entries, count, &error) == 0);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&entries[index];
		size_t binding;

		if (strncmp(entry->id, "kunit:branch-control-", 21))
			continue;
		EXPECT(entry->classification_mask ==
		       ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0);
		EXPECT(entry->obligations ==
		       (ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC));
		EXPECT(entry->unproved_obligations == entry->obligations);
		EXPECT(entry->linux_interface ==
		       ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE);
		entry_count++;
		binding_count += entry->binding_count;
		for (binding = 0; binding < entry->binding_count; binding++) {
			const struct orlix_tcti_target_proof_binding *item =
				&entry->bindings[binding];

			for (expected_index = 0;
			     expected_index < sizeof(expected) / sizeof(expected[0]);
			     expected_index++)
				if (expected[expected_index].ordinal == item->source_ordinal)
					break;
			EXPECT(expected_index < sizeof(expected) / sizeof(expected[0]));
			if (expected_index == sizeof(expected) / sizeof(expected[0]))
				continue;
			EXPECT(!strcmp(entry->id, expected[expected_index].proof_id));
			EXPECT(!strcmp(item->leaf_name,
				       expected[expected_index].leaf_name));
			EXPECT(!strcmp(item->mnemonic,
				       expected[expected_index].mnemonic));
			EXPECT(!strcmp(entry->operation_id,
				       expected[expected_index].operation_id));
		}
	}
	EXPECT(entry_count == 13);
	EXPECT(binding_count == 15);
	return 0;
}

static int source_registration_discharges_zero_semantic_obligations(void)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&entries[index];

		EXPECT(entry->obligations != 0);
		EXPECT(entry->unproved_obligations == entry->obligations);
		EXPECT((entry->obligations & ~entry->unproved_obligations) == 0);
	}
	return 0;
}

static int scalar_bitops_registry_binds_exact_source_rows(void)
{
	static const struct {
		uint32_t ordinal;
		const char *proof_id;
		const char *leaf_name;
		const char *mnemonic;
		const char *operation_id;
	} expected[] = {
		{ 3389U, "kunit:scalar-bitops-rbit", "RBIT_32_dp_1src",
		  "RBIT", "RBIT_int" },
		{ 3390U, "kunit:scalar-bitops-rev16", "REV16_32_dp_1src",
		  "REV16", "REV16_int" },
		{ 3391U, "kunit:scalar-bitops-rev", "REV_32_dp_1src",
		  "REV", "REV" },
		{ 3392U, "kunit:scalar-bitops-clz", "CLZ_32_dp_1src",
		  "CLZ", "CLZ_int" },
		{ 3393U, "kunit:scalar-bitops-cls", "CLS_32_dp_1src",
		  "CLS", "CLS_int" },
		{ 3397U, "kunit:scalar-bitops-rbit", "RBIT_64_dp_1src",
		  "RBIT", "RBIT_int" },
		{ 3398U, "kunit:scalar-bitops-rev16", "REV16_64_dp_1src",
		  "REV16", "REV16_int" },
		{ 3399U, "kunit:scalar-bitops-rev32", "REV32_64_dp_1src",
		  "REV32", "REV32_int" },
		{ 3400U, "kunit:scalar-bitops-rev", "REV_64_dp_1src",
		  "REV", "REV" },
		{ 3401U, "kunit:scalar-bitops-clz", "CLZ_64_dp_1src",
		  "CLZ", "CLZ_int" },
		{ 3402U, "kunit:scalar-bitops-cls", "CLS_64_dp_1src",
		  "CLS", "CLS_int" },
	};
	const struct orlix_tcti_target_proof_registry_entry *entries;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	for (index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry = NULL;
		const struct orlix_tcti_target_proof_binding *binding = NULL;
		size_t entry_index;
		size_t binding_index;

		for (entry_index = 0; entry_index < count; entry_index++) {
			if (!strcmp(entries[entry_index].id, expected[index].proof_id)) {
				entry = &entries[entry_index];
				break;
			}
		}
		EXPECT(entry != NULL);
		EXPECT(!strcmp(entry->operation_id, expected[index].operation_id));
		EXPECT(!strcmp(entry->kunit_source,
			"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_scalar_bitops_source_bound_test.c"));
		EXPECT(!strcmp(entry->kunit_suite,
			"orlix-tcti-scalar-bitops-source-bound"));
		EXPECT(entry->kunit_case_count == 1);
		EXPECT(!strcmp(entry->kunit_cases[0].name,
			"orlix_tcti_scalar_bitops_source_bindings"));
		EXPECT(entry->kunit_cases[0].obligations ==
			(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
			 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS));
		EXPECT(entry->unproved_obligations == entry->obligations);
		EXPECT(entry->obligations &
		       ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS);
		EXPECT(entry->unproved_obligations &
		       ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS);
		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++) {
			if (entry->bindings[binding_index].source_ordinal ==
			    expected[index].ordinal) {
				binding = &entry->bindings[binding_index];
				break;
			}
		}
		EXPECT(binding != NULL);
		EXPECT(!strcmp(binding->leaf_name, expected[index].leaf_name));
		EXPECT(!strcmp(binding->mnemonic, expected[index].mnemonic));
	}
	return 0;
}

static int scalar_fp_convert_registry_binds_exact_source_rows(void)
{
	static const struct {
		uint32_t ordinal;
		const char *proof_id;
		const char *leaf_name;
		const char *mnemonic;
		const char *operation_id;
	} expected[] = {
		{ 4084U, "kunit:scalar-fp-convert-scvtf-fix",
		  "SCVTF_S32_float2fix", "SCVTF", "SCVTF_float_fix" },
		{ 4085U, "kunit:scalar-fp-convert-ucvtf-fix",
		  "UCVTF_S32_float2fix", "UCVTF", "UCVTF_float_fix" },
		{ 4086U, "kunit:scalar-fp-convert-fcvtzs-fix",
		  "FCVTZS_32S_float2fix", "FCVTZS", "FCVTZS_float_fix" },
		{ 4087U, "kunit:scalar-fp-convert-fcvtzu-fix",
		  "FCVTZU_32S_float2fix", "FCVTZU", "FCVTZU_float_fix" },
		{ 4108U, "kunit:scalar-fp-convert-fcvtns",
		  "FCVTNS_32S_float2int", "FCVTNS", "FCVTNS_float" },
		{ 4109U, "kunit:scalar-fp-convert-fcvtnu",
		  "FCVTNU_32S_float2int", "FCVTNU", "FCVTNU_float" },
		{ 4110U, "kunit:scalar-fp-convert-scvtf-int",
		  "SCVTF_S32_float2int", "SCVTF", "SCVTF_float_int" },
		{ 4111U, "kunit:scalar-fp-convert-ucvtf-int",
		  "UCVTF_S32_float2int", "UCVTF", "UCVTF_float_int" },
		{ 4112U, "kunit:scalar-fp-convert-fcvtas",
		  "FCVTAS_32S_float2int", "FCVTAS", "FCVTAS_float" },
		{ 4113U, "kunit:scalar-fp-convert-fcvtau",
		  "FCVTAU_32S_float2int", "FCVTAU", "FCVTAU_float" },
		{ 4116U, "kunit:scalar-fp-convert-fcvtps",
		  "FCVTPS_32S_float2int", "FCVTPS", "FCVTPS_float" },
		{ 4117U, "kunit:scalar-fp-convert-fcvtpu",
		  "FCVTPU_32S_float2int", "FCVTPU", "FCVTPU_float" },
		{ 4118U, "kunit:scalar-fp-convert-fcvtms",
		  "FCVTMS_32S_float2int", "FCVTMS", "FCVTMS_float" },
		{ 4119U, "kunit:scalar-fp-convert-fcvtmu",
		  "FCVTMU_32S_float2int", "FCVTMU", "FCVTMU_float" },
		{ 4120U, "kunit:scalar-fp-convert-fcvtzs-int",
		  "FCVTZS_32S_float2int", "FCVTZS", "FCVTZS_float_int" },
		{ 4121U, "kunit:scalar-fp-convert-fcvtzu-int",
		  "FCVTZU_32S_float2int", "FCVTZU", "FCVTZU_float_int" },
	};
	const struct orlix_tcti_target_proof_registry_entry *entries;
	size_t count;
	size_t expected_index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	for (expected_index = 0; expected_index < sizeof(expected) / sizeof(expected[0]);
	     expected_index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry = NULL;
		const struct orlix_tcti_target_proof_binding *binding;
		struct orlix_tcti_target_proof_reference reference;
		uint32_t requirements;
		size_t entry_index;

		EXPECT(orlix_tcti_target_proof_operation_requirements(
			       expected[expected_index].operation_id, 1,
			       &requirements) == 0);
		EXPECT(requirements == SCALAR_FP_CONVERT_OBLIGATIONS);
		for (entry_index = 0; entry_index < count; entry_index++) {
			if (!strcmp(entries[entry_index].id,
				    expected[expected_index].proof_id)) {
				entry = &entries[entry_index];
				break;
			}
		}
		EXPECT(entry != NULL);
		EXPECT(!strcmp(entry->operation_id,
			       expected[expected_index].operation_id));
		EXPECT(entry->classification_mask == ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0);
		EXPECT(entry->obligations == SCALAR_FP_CONVERT_OBLIGATIONS);
		EXPECT(entry->unproved_obligations == SCALAR_FP_CONVERT_OBLIGATIONS);
		EXPECT(entry->binding_count == 1);
		binding = entry->bindings;
		EXPECT(binding->source_ordinal == expected[expected_index].ordinal);
		EXPECT(!strcmp(binding->leaf_name, expected[expected_index].leaf_name));
		EXPECT(!strcmp(binding->mnemonic, expected[expected_index].mnemonic));
		reference = (struct orlix_tcti_target_proof_reference){
			.id = expected[expected_index].proof_id,
			.leaf_name = expected[expected_index].leaf_name,
			.mnemonic = expected[expected_index].mnemonic,
			.operation_id = expected[expected_index].operation_id,
			.encoding_mask = binding->encoding_mask,
			.encoding_pattern = binding->encoding_pattern,
			.condition_tcnd_hex = binding->condition_tcnd_hex,
			.classification = 1,
		};
		EXPECT(orlix_tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
	}
	return 0;
}

static int integer_conditional_registry_binds_exact_source_rows(void)
{
	static const struct {
		uint32_t ordinal;
		const char *proof_id;
		const char *leaf_name;
		const char *mnemonic;
		const char *operation_id;
		uint32_t mask;
		uint32_t pattern;
		bool flags;
	} expected[] = {
		{ 3356U, "kunit:integer-conditional-udiv", "UDIV_32_dp_2src", "UDIV", "UDIV", 0xffe0fc00U, 0x1ac00800U, 0 },
		{ 3357U, "kunit:integer-conditional-sdiv", "SDIV_32_dp_2src", "SDIV", "SDIV", 0xffe0fc00U, 0x1ac00c00U, 0 },
		{ 3373U, "kunit:integer-conditional-udiv", "UDIV_64_dp_2src", "UDIV", "UDIV", 0xffe0fc00U, 0x9ac00800U, 0 },
		{ 3374U, "kunit:integer-conditional-sdiv", "SDIV_64_dp_2src", "SDIV", "SDIV", 0xffe0fc00U, 0x9ac00c00U, 0 },
		{ 3479U, "kunit:integer-conditional-ccmn-reg", "CCMN_32_condcmp_reg", "CCMN", "CCMN_reg", 0xffe00c10U, 0x3a400000U, true },
		{ 3480U, "kunit:integer-conditional-ccmp-reg", "CCMP_32_condcmp_reg", "CCMP", "CCMP_reg", 0xffe00c10U, 0x7a400000U, true },
		{ 3481U, "kunit:integer-conditional-ccmn-reg", "CCMN_64_condcmp_reg", "CCMN", "CCMN_reg", 0xffe00c10U, 0xba400000U, true },
		{ 3482U, "kunit:integer-conditional-ccmp-reg", "CCMP_64_condcmp_reg", "CCMP", "CCMP_reg", 0xffe00c10U, 0xfa400000U, true },
		{ 3483U, "kunit:integer-conditional-ccmn-imm", "CCMN_32_condcmp_imm", "CCMN", "CCMN_imm", 0xffe00c10U, 0x3a400800U, true },
		{ 3484U, "kunit:integer-conditional-ccmp-imm", "CCMP_32_condcmp_imm", "CCMP", "CCMP_imm", 0xffe00c10U, 0x7a400800U, true },
		{ 3485U, "kunit:integer-conditional-ccmn-imm", "CCMN_64_condcmp_imm", "CCMN", "CCMN_imm", 0xffe00c10U, 0xba400800U, true },
		{ 3486U, "kunit:integer-conditional-ccmp-imm", "CCMP_64_condcmp_imm", "CCMP", "CCMP_imm", 0xffe00c10U, 0xfa400800U, true },
		{ 3487U, "kunit:integer-conditional-csel", "CSEL_32_condsel", "CSEL", "CSEL", 0xffe00c00U, 0x1a800000U, false },
		{ 3488U, "kunit:integer-conditional-csinc", "CSINC_32_condsel", "CSINC", "CSINC", 0xffe00c00U, 0x1a800400U, false },
		{ 3489U, "kunit:integer-conditional-csinv", "CSINV_32_condsel", "CSINV", "CSINV", 0xffe00c00U, 0x5a800000U, false },
		{ 3490U, "kunit:integer-conditional-csneg", "CSNEG_32_condsel", "CSNEG", "CSNEG", 0xffe00c00U, 0x5a800400U, false },
		{ 3491U, "kunit:integer-conditional-csel", "CSEL_64_condsel", "CSEL", "CSEL", 0xffe00c00U, 0x9a800000U, false },
		{ 3492U, "kunit:integer-conditional-csinc", "CSINC_64_condsel", "CSINC", "CSINC", 0xffe00c00U, 0x9a800400U, false },
		{ 3493U, "kunit:integer-conditional-csinv", "CSINV_64_condsel", "CSINV", "CSINV", 0xffe00c00U, 0xda800000U, false },
		{ 3494U, "kunit:integer-conditional-csneg", "CSNEG_64_condsel", "CSNEG", "CSNEG", 0xffe00c00U, 0xda800400U, false },
		{ 3495U, "kunit:integer-conditional-madd", "MADD_32A_dp_3src", "MADD", "MADD", 0xffe08000U, 0x1b000000U, false },
		{ 3496U, "kunit:integer-conditional-msub", "MSUB_32A_dp_3src", "MSUB", "MSUB", 0xffe08000U, 0x1b008000U, false },
		{ 3497U, "kunit:integer-conditional-madd", "MADD_64A_dp_3src", "MADD", "MADD", 0xffe08000U, 0x9b000000U, false },
		{ 3498U, "kunit:integer-conditional-msub", "MSUB_64A_dp_3src", "MSUB", "MSUB", 0xffe08000U, 0x9b008000U, false },
		{ 3499U, "kunit:integer-conditional-smaddl", "SMADDL_64WA_dp_3src", "SMADDL", "SMADDL", 0xffe08000U, 0x9b200000U, false },
		{ 3500U, "kunit:integer-conditional-smsubl", "SMSUBL_64WA_dp_3src", "SMSUBL", "SMSUBL", 0xffe08000U, 0x9b208000U, false },
		{ 3501U, "kunit:integer-conditional-smulh", "SMULH_64_dp_3src", "SMULH", "SMULH", 0xffe0fc00U, 0x9b407c00U, false },
		{ 3504U, "kunit:integer-conditional-umaddl", "UMADDL_64WA_dp_3src", "UMADDL", "UMADDL", 0xffe08000U, 0x9ba00000U, false },
		{ 3505U, "kunit:integer-conditional-umsubl", "UMSUBL_64WA_dp_3src", "UMSUBL", "UMSUBL", 0xffe08000U, 0x9ba08000U, false },
		{ 3506U, "kunit:integer-conditional-umulh", "UMULH_64_dp_3src", "UMULH", "UMULH", 0xffe0fc00U, 0x9bc07c00U, false },
	};
	const struct orlix_tcti_target_proof_registry_entry *entries;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	for (index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
		struct orlix_tcti_target_proof_reference reference = {
			expected[index].proof_id, expected[index].leaf_name,
			expected[index].mnemonic, expected[index].operation_id,
			expected[index].mask, expected[index].pattern, TRUE_CONDITION, 1,
		};
		uint32_t requirements;

		EXPECT(orlix_tcti_target_proof_operation_requirements(
			expected[index].operation_id, 1, &requirements) == 0);
		EXPECT(requirements == (expected[index].flags ?
			INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS :
			INTEGER_CONDITIONAL_OBLIGATIONS));
		EXPECT(orlix_tcti_target_proof_registry_lookup(entries, count, &reference) ==
			ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK);
	}
	return 0;
}

static int linux_proof_matrix_is_lossless_and_fail_closed(void)
{
	const struct orlix_tcti_target_linux_proof_disposition_row *canonical;
	struct orlix_tcti_target_linux_proof_disposition_row *mutated;
	struct orlix_tcti_target_linux_proof_matrix_result result;
	struct orlix_tcti_target_kselftest_provenance substitution;
	struct orlix_tcti_target_linux_proof_source_identity synthetic;
	size_t applicable = 0;
	size_t not_applicable = 0;
	size_t prefetch_hints = 0;
	size_t count;
	size_t index;

	canonical = orlix_tcti_target_linux_proof_dispositions(&count);
	EXPECT(canonical != NULL);
	EXPECT(count == ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS);
	EXPECT(orlix_tcti_target_linux_proof_matrix_validate(canonical, count,
							     &result) == 0);
	EXPECT(result.error_mask == ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_NONE);
	EXPECT(result.source_leaf_rows == 4350U);
	EXPECT(result.semantic_variant_rows == 2014U);
	EXPECT(result.kselftest_owned_rows + result.not_applicable_rows == count);
	EXPECT(result.executed_kselftest_rows == 0U);
	for (index = 0; index < count; index++) {
		if (canonical[index].disposition ==
		    ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE)
			EXPECT(canonical[index].not_applicable_reason ==
				       ORLIX_TCTI_TARGET_LINUX_NA_PREFETCH_HINT ||
			       canonical[index].not_applicable_reason ==
				       ORLIX_TCTI_TARGET_LINUX_NA_ARCHITECTURAL_SEMANTICS_ONLY);
		if (canonical[index].subject_kind ==
			    ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF &&
		    !strcmp(canonical[index].source.mnemonic, "PRFM")) {
			EXPECT(canonical[index].disposition ==
			       ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE);
			EXPECT(canonical[index].not_applicable_reason ==
			       ORLIX_TCTI_TARGET_LINUX_NA_PREFETCH_HINT);
			prefetch_hints++;
		}
	}
	EXPECT(prefetch_hints == 3U);
	EXPECT(orlix_tcti_target_linux_source_policy_validate_for_test(
		       &canonical[0].source) == 0);
	synthetic = canonical[0].source;
	synthetic.source_index = ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS;
	synthetic.name = "synthetic_unclassified_leaf";
	synthetic.mnemonic = "UNMATCHED";
	synthetic.operation_id = "unmatched_operation";
	EXPECT(orlix_tcti_target_linux_source_policy_validate_for_test(
		       &synthetic) == -1);
	synthetic = canonical[0].source;
	synthetic.mnemonic = "UNMATCHED";
	synthetic.operation_id = "unmatched_operation";
	EXPECT(orlix_tcti_target_linux_source_policy_validate_for_test(
		       &synthetic) == -1);
	mutated = malloc(count * sizeof(*mutated));
	EXPECT(mutated != NULL);
	memcpy(mutated, canonical, count * sizeof(*mutated));

	EXPECT(orlix_tcti_target_linux_proof_matrix_validate(mutated, count - 1,
							     &result) == -1);
	EXPECT(result.missing_rows == 1U);
	EXPECT(result.error_mask & ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_COUNT);
	EXPECT(result.error_mask & ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_MISSING);

	memcpy(mutated, canonical, count * sizeof(*mutated));
	mutated[1] = mutated[0];
	EXPECT(orlix_tcti_target_linux_proof_matrix_validate(mutated, count,
							     &result) == -1);
	EXPECT(result.duplicate_rows == 1U);
	EXPECT(result.missing_rows == 1U);

	memcpy(mutated, canonical, count * sizeof(*mutated));
	mutated[0].source.source_length++;
	EXPECT(orlix_tcti_target_linux_proof_matrix_validate(mutated, count,
							     &result) == -1);
	EXPECT(result.stale_rows == 1U);

	for (index = 0; index < count; index++) {
		if (!applicable && canonical[index].disposition ==
		    ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED)
			applicable = index + 1U;
		if (!not_applicable && canonical[index].disposition ==
		    ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE)
			not_applicable = index + 1U;
	}
	EXPECT(applicable != 0U);
	EXPECT(not_applicable != 0U);

	memcpy(mutated, canonical, count * sizeof(*mutated));
	index = applicable - 1U;
	substitution = mutated[index].kselftests[0];
	substitution.source_sha256 =
		"0000000000000000000000000000000000000000000000000000000000000000";
	mutated[index].kselftests = &substitution;
	mutated[index].kselftest_count = 1U;
	EXPECT(orlix_tcti_target_linux_proof_matrix_validate(mutated, count,
							     &result) == -1);
	EXPECT(result.invalid_provenance_rows == 1U);

	memcpy(mutated, canonical, count * sizeof(*mutated));
	substitution = mutated[index].kselftests[0];
	substitution.source =
		"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/fabricated_kunit.c";
	mutated[index].kselftests = &substitution;
	mutated[index].kselftest_count = 1U;
	EXPECT(orlix_tcti_target_linux_proof_matrix_validate(mutated, count,
							     &result) == -1);
	EXPECT(result.substitution_rows == 1U);

	memcpy(mutated, canonical, count * sizeof(*mutated));
	index = not_applicable - 1U;
	mutated[index].not_applicable_reason = ORLIX_TCTI_TARGET_LINUX_NA_NONE;
	EXPECT(orlix_tcti_target_linux_proof_matrix_validate(mutated, count,
							     &result) == -1);
	EXPECT(result.ambiguous_rows == 1U);
	free(mutated);
	return 0;
}

static int operational_note_mapping_is_exact_and_fail_closed(void)
{
	static const char identity[] =
		"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe:12:7";
	static const char digest[] =
		"0000000000000000000000000000000000000000000000000000000000000000";
	static const u8 strings[] =
		"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe:12:7\0"
		"0000000000000000000000000000000000000000000000000000000000000000\0";
	static const struct orlix_tcti_target_instruction_artifact_operational_note note = {
		.leaf_index = 2U,
		.source_offset = 12U,
		.source_length = 7U,
		.source_identity_offset = 0U,
		.source_sha256_offset = sizeof(identity),
		.kind = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_BEHAVIOR_OBLIGATION,
	};
	static const struct orlix_tcti_target_instruction_artifact artifact = {
		.leaf_count = 3U,
		.operational_notes = &note,
		.operational_note_count = 1U,
		.string_pool = strings,
		.string_pool_size = sizeof(strings),
	};
	static const struct orlix_tcti_target_proof_case proof_case = {
		.name = "note_case",
		.obligations = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE,
	};
	static const struct orlix_tcti_target_proof_binding proof_binding = {
		.kunit_case_mask = ORLIX_TCTI_PROOF_U64_C(1),
		.source_ordinal = 2U,
	};
	static const struct orlix_tcti_target_proof_registry_entry registry = {
		.id = "note-proof",
		.obligations = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE,
		.kunit_cases = &proof_case,
		.kunit_case_count = 1U,
		.bindings = &proof_binding,
		.binding_count = 1U,
	};
	struct orlix_tcti_target_operational_note_proof_mapping mappings[2] = { {
		.leaf_index = 2U,
		.source_identity = identity,
		.source_sha256 = digest,
		.proof_id = "note-proof",
		.kunit_case_name = "note_case",
	} };
	struct orlix_tcti_target_operational_note_mapping_result result;

	EXPECT(!orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 1U, &registry, 1U, &result));
	EXPECT(result.mapped_count == 1U);
	mappings[0].source_sha256 =
		"1111111111111111111111111111111111111111111111111111111111111111";
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 1U, &registry, 1U, &result) == -1);
	EXPECT(result.error ==
		ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_DIGEST_MISMATCH);
	mappings[0].source_sha256 = digest;
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, NULL, 0U, &registry, 1U, &result) == -1);
	EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MISSING);
	mappings[1] = mappings[0];
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 2U, &registry, 1U, &result) == -1);
	EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_DUPLICATE);
	mappings[0].leaf_index = 1U;
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 1U, &registry, 1U, &result) == -1);
	EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS);
	mappings[0].leaf_index = 2U;
	mappings[1] = (struct orlix_tcti_target_operational_note_proof_mapping) {
		.leaf_index = 0U,
		.source_identity = "stale",
		.source_sha256 = digest,
		.proof_id = "note-proof",
		.kunit_case_name = "note_case",
	};
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 2U, &registry, 1U, &result) == -1);
	EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_STALE);
	mappings[0].proof_id = "unknown";
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 1U, &registry, 1U, &result) == -1);
	EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_UNKNOWN_PROOF);
	mappings[0].proof_id = "note-proof";
	mappings[0].kunit_case_name = "unknown";
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 1U, &registry, 1U, &result) == -1);
	EXPECT(result.error ==
		ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_UNKNOWN_NATIVE_CASE);
	mappings[0].kunit_case_name = NULL;
	EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
		&artifact, mappings, 1U, &registry, 1U, &result) == -1);
	EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MALFORMED);
	{
		struct orlix_tcti_target_proof_binding wrong_ordinal = proof_binding;
		struct orlix_tcti_target_proof_registry_entry mutated = registry;

		mappings[0].kunit_case_name = "note_case";
		wrong_ordinal.source_ordinal = 1U;
		mutated.bindings = &wrong_ordinal;
		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, &mutated, 1U, &result) == -1);
		EXPECT(result.error ==
			ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_BINDING_MISMATCH);
	}
	{
		struct orlix_tcti_target_proof_registry_entry mutated = registry;

		mutated.bindings = NULL;
		mutated.binding_count = 0U;
		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, &mutated, 1U, &result) == -1);
		EXPECT(result.error ==
			ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_BINDING_MISMATCH);
	}
	{
		struct orlix_tcti_target_proof_binding unrelated_case = proof_binding;
		struct orlix_tcti_target_proof_registry_entry mutated = registry;

		unrelated_case.kunit_case_mask = 0U;
		mutated.bindings = &unrelated_case;
		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, &mutated, 1U, &result) == -1);
		EXPECT(result.error ==
			ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_BINDING_MISMATCH);
	}
	{
		struct orlix_tcti_target_proof_case unrelated_case = proof_case;
		struct orlix_tcti_target_proof_registry_entry mutated = registry;

		unrelated_case.obligations =
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS;
		mutated.kunit_cases = &unrelated_case;
		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, &mutated, 1U, &result) == -1);
		EXPECT(result.error ==
			ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_INSUFFICIENT_OBLIGATIONS);
	}
	{
		struct orlix_tcti_target_proof_registry_entry mutated = registry;

		mutated.obligations = 0U;
		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, &mutated, 1U, &result) == -1);
		EXPECT(result.error ==
			ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_INSUFFICIENT_OBLIGATIONS);
	}
	{
		struct orlix_tcti_target_proof_case duplicate_cases[] = {
			proof_case, proof_case,
		};
		struct orlix_tcti_target_proof_registry_entry mutated = registry;

		mutated.kunit_cases = duplicate_cases;
		mutated.kunit_case_count = 2U;
		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, &mutated, 1U, &result) == -1);
		EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS);
	}
	{
		struct orlix_tcti_target_proof_binding duplicate_bindings[] = {
			proof_binding, proof_binding,
		};
		struct orlix_tcti_target_proof_registry_entry mutated = registry;

		mutated.bindings = duplicate_bindings;
		mutated.binding_count = 2U;
		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, &mutated, 1U, &result) == -1);
		EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS);
	}
	{
		struct orlix_tcti_target_proof_registry_entry duplicate_proofs[] = {
			registry, registry,
		};

		EXPECT(orlix_tcti_target_operational_note_proof_mappings_validate(
			&artifact, mappings, 1U, duplicate_proofs, 2U, &result) == -1);
		EXPECT(result.error == ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS);
	}
	return 0;
}

static int advsimd_structure_registry_accounts_for_exact_issue_126_cohort(void)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	const struct orlix_tcti_target_linux_proof_disposition_row *rows;
	enum orlix_tcti_target_proof_registry_error error;
	size_t entry_count;
	size_t row_count;
	size_t binding_count = 0;
	size_t disposition_count = 0;
	unsigned int ordinal;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&entry_count);
	rows = orlix_tcti_target_linux_proof_dispositions(&row_count);
	EXPECT(entries && rows);
	EXPECT(!orlix_tcti_target_proof_registry_source_bound_projection_validate(
		entries, entry_count, &error));
	for (ordinal = 2353U; ordinal <= 2504U; ordinal++) {
		size_t binding_matches = 0;
		size_t disposition_matches = 0;

		for (index = 0; index < entry_count; index++) {
			size_t binding;

			if (strncmp(entries[index].id,
				    "kunit:advsimd-structure-",
				    strlen("kunit:advsimd-structure-")))
				continue;
			for (binding = 0; binding < entries[index].binding_count;
			     binding++)
				if (entries[index].bindings[binding].source_ordinal ==
				    ordinal)
					binding_matches++;
		}
		for (index = 0; index < row_count; index++) {
			if (rows[index].subject_kind !=
			    ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF ||
			    rows[index].source.source_index != ordinal)
				continue;
			disposition_matches++;
			EXPECT(rows[index].disposition ==
				ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED ||
			       rows[index].disposition ==
				ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE);
			if (rows[index].disposition ==
			    ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED)
				EXPECT(rows[index].kselftests &&
				       rows[index].kselftest_count);
			else
				EXPECT(rows[index].not_applicable_reason !=
				       ORLIX_TCTI_TARGET_LINUX_NA_NONE);
		}
		EXPECT(binding_matches == 1U);
		EXPECT(disposition_matches == 1U);
		binding_count += binding_matches;
		disposition_count += disposition_matches;
	}
	EXPECT(binding_count == 152U);
	EXPECT(disposition_count == 152U);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "operational_note_mapping_is_exact_and_fail_closed",
		 operational_note_mapping_is_exact_and_fail_closed },
		{ "advsimd_structure_registry_accounts_for_exact_issue_126_cohort",
		  advsimd_structure_registry_accounts_for_exact_issue_126_cohort },
		{ "canonical_first_use_is_concurrent_and_immutable",
		  canonical_first_use_is_concurrent_and_immutable },
		{ "real_manifest_bindings_are_exact",
		  real_manifest_bindings_are_exact },
		{ "registry_fails_closed_on_unknown_operations_and_bits",
		  registry_fails_closed_on_unknown_operations_and_bits },
		{ "required_operation_maps_once", required_operation_maps_once },
		{ "explicit_binding_set_is_mandatory_and_unique",
		  explicit_binding_set_is_mandatory_and_unique },
		{ "cross_operation_multi_binding_is_rejected",
		  cross_operation_multi_binding_is_rejected },
		{ "lse_cannot_omit_atomicity", lse_cannot_omit_atomicity },
		{ "kunit_provenance_must_be_registered",
		  kunit_provenance_must_be_registered },
		{ "registered_case_cannot_overclaim_obligations",
		  registered_case_cannot_overclaim_obligations },
		{ "typed_kselftest_provenance_is_source_and_build_bound",
		  typed_kselftest_provenance_is_source_and_build_bound },
		{ "privileged_profiles_and_count_caps_fail_closed",
		  privileged_profiles_and_count_caps_fail_closed },
		{ "logical_shifted_register_registry_is_source_bound",
		  logical_shifted_register_registry_is_source_bound },
		{ "cssc_min_max_immediate_registry_is_source_bound",
		  cssc_min_max_immediate_registry_is_source_bound },
		{ "add_sub_immediate_registry_is_source_bound",
		  add_sub_immediate_registry_is_source_bound },
		{ "lse_registry_binds_180_leaves_without_claiming_completion",
		  lse_registry_binds_180_leaves_without_claiming_completion },
		{ "lse_registry_cannot_clear_unproved_duties_statically",
		  lse_registry_cannot_clear_unproved_duties_statically },
		{ "scalar_source_bindings_are_complete_and_fail_closed",
		  scalar_source_bindings_are_complete_and_fail_closed },
		{ "scalar_bitops_registry_binds_exact_source_rows",
		  scalar_bitops_registry_binds_exact_source_rows },
		{ "exclusive_registry_binds_exact_baseline_leaves",
		  exclusive_registry_binds_exact_baseline_leaves },
		{ "branch_control_registry_binds_exact_source_rows",
		  branch_control_registry_binds_exact_source_rows },
		{ "source_registration_discharges_zero_semantic_obligations",
		  source_registration_discharges_zero_semantic_obligations },
		{ "scalar_fp_convert_registry_binds_exact_source_rows",
		  scalar_fp_convert_registry_binds_exact_source_rows },
		{ "integer_conditional_registry_binds_exact_source_rows",
		  integer_conditional_registry_binds_exact_source_rows },
		{ "linux_proof_matrix_is_lossless_and_fail_closed",
		  linux_proof_matrix_is_lossless_and_fail_closed },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run())
			return 1;
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
