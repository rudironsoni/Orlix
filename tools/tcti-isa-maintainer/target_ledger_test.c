/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_ledger.h"

#include <stdio.h>
#include <string.h>

#define EXPECT(value) do { \
	if (!(value)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); \
		return -1; \
	} \
} while (0)

#define TRUE_CONDITION \
	"54434e440107000000220700000017070000000c010000000101010000000101010000000101010000000101"
#define DECODE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c"
#define ADD_OBLIGATIONS \
	(TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 TCTI_TARGET_PROOF_OBLIGATION_PC)

static const struct tcti_target_ledger_source_row source[] = {
	{ 0, "ADD_32_addsub_imm", "ADD", "ADD_addsub_imm",
	  0xff800000U, 0x11000000U, TRUE_CONDITION },
	{ 1, "ADDS_32S_addsub_imm", "ADDS", "ADDS_addsub_imm",
	  0xff800000U, 0x31000000U, TRUE_CONDITION },
};
static struct tcti_target_ledger_classification_row classification[] = {
	{ "ADD_32_addsub_imm", TCTI_TARGET_LEDGER_REQUIRED_EL0,
	  TCTI_A64_TARGET_RELATION_NONE, "", "decode-and-semantics",
	  "kunit:add-sub-immediate", NULL, NULL, NULL },
	{ "ADDS_32S_addsub_imm", TCTI_TARGET_LEDGER_REQUIRED_EL0,
	  TCTI_A64_TARGET_RELATION_NONE, "", "decode-and-semantics",
	  "kunit:adds-immediate", NULL, NULL, NULL },
};
static const struct tcti_target_proof_binding add_binding[] = {
	{ "ADD_32_addsub_imm", "ADD", 0xff800000U, 0x11000000U,
	  TRUE_CONDITION, UINT64_C(1) },
	};
static const struct tcti_target_proof_binding adds_binding[] = {
	{ "ADDS_32S_addsub_imm", "ADDS", 0xff800000U, 0x31000000U,
	  TRUE_CONDITION, UINT64_C(1) },
};
static const struct tcti_target_proof_case add_cases[] = {
	{ "tcti_gadget_executes_complete_add_sub_immediate_family",
	  ADD_OBLIGATIONS },
};
static const struct tcti_target_proof_case adds_cases[] = {
	{ "tcti_gadget_executes_complete_add_sub_immediate_family",
	  ADD_OBLIGATIONS | TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
};
static const struct tcti_target_proof_registry_entry proof_registry[] = {
	{ "kunit:add-sub-immediate", "ADD_addsub_imm",
	  TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, ADD_OBLIGATIONS,
	  TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
	  DECODE_SOURCE, "orlix-tcti-decode", add_cases, 1, add_binding, 1,
	  NULL },
	{ "kunit:adds-immediate", "ADDS_addsub_imm",
	  TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
	  ADD_OBLIGATIONS | TCTI_TARGET_PROOF_OBLIGATION_FLAGS,
	  TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
	  DECODE_SOURCE, "orlix-tcti-decode", adds_cases, 1, adds_binding, 1,
	  NULL },
};

static int validate(const struct tcti_target_ledger_classification_row *rows,
		    struct tcti_target_ledger_result *result)
{
	return tcti_target_ledger_validate(source, 2, rows, 2, 2,
					   proof_registry, 2, result);
}

static int real_manifest_rows_pass_exact_binding(void)
{
	struct tcti_target_ledger_result result;

	EXPECT(validate(classification, &result) == 0);
	EXPECT(result.classified_rows == 2);
	EXPECT(result.proof_gaps == 0);
	return 0;
}

static int classification_order_does_not_change_binding(void)
{
	struct tcti_target_ledger_classification_row reversed[] = {
		classification[1], classification[0],
	};
	struct tcti_target_ledger_result result;

	EXPECT(validate(reversed, &result) == 0);
	return 0;
}

static int every_required_operation_maps_exactly_once(void)
{
	struct tcti_target_ledger_result result;

	EXPECT(tcti_target_ledger_validate(source, 2, classification, 2, 2,
					  NULL, 0, &result) == -1);
	EXPECT(result.first_error ==
	       TCTI_TARGET_LEDGER_REQUIRED_OPERATION_PROOF_COUNT);
	return 0;
}

static int every_required_operation_has_explicit_requirements(void)
{
	struct tcti_target_ledger_source_row unmapped[2];
	struct tcti_target_ledger_result result;

	memcpy(unmapped, source, sizeof(unmapped));
	unmapped[0].operation_id = "UNMAPPED_REQUIRED_OPERATION";
	EXPECT(tcti_target_ledger_validate(unmapped, 2, classification, 2, 2,
					  NULL, 0, &result) == -1);
	EXPECT(result.first_error ==
	       TCTI_TARGET_LEDGER_UNKNOWN_OPERATION_REQUIREMENTS);
	return 0;
}

static int leaf_fingerprint_drift_fails(void)
{
	struct tcti_target_ledger_source_row changed[2];
	struct tcti_target_ledger_result result;

	memcpy(changed, source, sizeof(changed));
	changed[1].pattern = 0x11000000U;
	EXPECT(tcti_target_ledger_validate(changed, 2, classification, 2, 2,
					  proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_STALE_PROOF_BINDING);
	return 0;
}

static int extra_registry_binding_must_resolve_to_source(void)
{
	struct tcti_target_proof_registry_entry registry[2] = {
		proof_registry[0], proof_registry[1],
	};
	struct tcti_target_proof_binding extra[] = {
		add_binding[0],
		{ "INVENTED_LEAF", "ADD", 0xff800000U, 0x11000000U,
		  TRUE_CONDITION, UINT64_C(1) },
	};
	struct tcti_target_ledger_result result;

	registry[0].bindings = extra;
	registry[0].binding_count = 2;
	EXPECT(tcti_target_ledger_validate(source, 2, classification, 2, 2,
					  registry, 2, &result) == -1);
	EXPECT(result.first_error ==
	       TCTI_TARGET_LEDGER_UNKNOWN_PROOF_BINDING_LEAF);
	return 0;
}

static int alias_requires_source_bound_equivalence(void)
{
	struct tcti_target_ledger_classification_row alias[2];
	struct tcti_target_ledger_result result;

	memcpy(alias, classification, sizeof(alias));
	alias[1].classification = TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE;
	alias[1].relation = TCTI_A64_TARGET_RELATION_ALIAS;
	alias[1].canonical_name = "ADD_32_addsub_imm";
	alias[1].proof = "kunit:add-sub-immediate";
	EXPECT(tcti_target_ledger_validate(source, 2, alias, 2, 2,
					  proof_registry, 1, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_MISSING_RELATION_EVIDENCE);
	alias[1].relation_evidence = "fixture relationship evidence";
	alias[1].relation_source =
		"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/target_ledger_test.c";
	alias[1].relation_source_sha256 =
		"0000000000000000000000000000000000000000000000000000000000000000";
	EXPECT(tcti_target_ledger_validate(source, 2, alias, 2, 2,
					  proof_registry, 1, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_INVALID_RELATION_EVIDENCE);
	return 0;
}

static int stale_alias_canonical_fails_without_indexing_past_source(void)
{
	struct tcti_target_ledger_classification_row stale[2] = {
		classification[0], classification[1],
	};
	struct tcti_target_ledger_result result;

	stale[0].classification = TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE;
	stale[0].relation = TCTI_A64_TARGET_RELATION_ALIAS;
	stale[0].canonical_name = "GHOST";
	stale[0].relation_evidence = "fixture relationship evidence";
	stale[1].name = "GHOST";
	EXPECT(tcti_target_ledger_validate(source, 2, stale, 2, 2, NULL, 0,
					  &result) == -1);
	EXPECT(result.errors > 0);
	return 0;
}

static int independent_array_counts_fail_before_indexing(void)
{
	struct tcti_target_ledger_result result;

	EXPECT(tcti_target_ledger_validate(source, 1, classification, 2, 2,
					   proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_BAD_COUNT);
	EXPECT(result.first_error_index == 1);
	EXPECT(tcti_target_ledger_validate(source, 3, classification, 2, 2,
					   proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_BAD_COUNT);
	EXPECT(result.first_error_index == 3);
	EXPECT(tcti_target_ledger_validate(source, 2, classification, 1, 2,
					   proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_BAD_COUNT);
	EXPECT(result.first_error_index == 1);
	EXPECT(tcti_target_ledger_validate(source, 2, classification, 3, 2,
					   proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_BAD_COUNT);
	EXPECT(result.first_error_index == 3);
	EXPECT(tcti_target_ledger_validate(NULL, 2, classification, 2, 2,
					   proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_BAD_COUNT);
	EXPECT(result.first_error_index == 2);
	EXPECT(tcti_target_ledger_validate(source, 2, NULL, 2, 2,
					   proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_BAD_COUNT);
	EXPECT(result.first_error_index == 2);
	return 0;
}

static int exact_target_count_and_import_freshness_remain_enforced(void)
{
	struct tcti_target_leaf leaves[2] = {
		{ "ADD_32_addsub_imm", "ADD", "ADD_addsub_imm", 0,
		  0x11000000U, TCTI_TARGET_EXPR_NONE },
		{ "ADDS_32S_addsub_imm", "ADDS", "ADDS_addsub_imm", 0xff800000U,
		  0x31000000U, TCTI_TARGET_EXPR_NONE },
	};
	struct tcti_target_inventory inventory = {
		.leaves = leaves,
		.leaf_count = 2,
	};
	struct tcti_target_ledger_result result = { 0 };

	EXPECT(tcti_target_ledger_validate(source, 2, classification, 2, 4350,
					  proof_registry, 2, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_BAD_COUNT);
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_ledger_verify_import(source, 2, &inventory, &result) == -1);
	EXPECT(result.first_error == TCTI_TARGET_LEDGER_STALE_SOURCE_FACT);
	return 0;
}

static int logical_shifted_register_rows_pass_production_registry(void)
{
	enum { FIRST_LOGICAL_SHIFT_ROW = 3434, LOGICAL_SHIFT_ROWS = 16 };
	struct tcti_target_ledger_source_row logical_source[LOGICAL_SHIFT_ROWS];
	struct tcti_target_ledger_classification_row
		logical_classification[LOGICAL_SHIFT_ROWS];
	const struct tcti_target_ledger_classification_row *all_classification;
	const struct tcti_target_ledger_source_row *all_source;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_ledger_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;
	size_t index;

	all_source = tcti_target_ledger_source(&source_count);
	all_classification =
		tcti_target_ledger_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	EXPECT(all_source != NULL);
	EXPECT(all_classification != NULL);
	EXPECT(source_count > FIRST_LOGICAL_SHIFT_ROW + LOGICAL_SHIFT_ROWS - 1);
	EXPECT(registry != NULL);
	EXPECT(registry_count == 8);
	for (index = 0; index < LOGICAL_SHIFT_ROWS; index++) {
		size_t classification_index;

		logical_source[index] =
			all_source[FIRST_LOGICAL_SHIFT_ROW + index];
		logical_source[index].ordinal = (uint32_t)index;
		for (classification_index = 0;
		     classification_index < classification_count;
		     classification_index++)
			if (!strcmp(all_classification[classification_index].name,
				    logical_source[index].name))
				break;
		EXPECT(classification_index < classification_count);
		logical_classification[index] =
			all_classification[classification_index];
		EXPECT(logical_classification[index].classification ==
		       TCTI_TARGET_LEDGER_REQUIRED_EL0);
		EXPECT(logical_classification[index].evidence != NULL);
		EXPECT(logical_classification[index].evidence[0] != '\0');
		EXPECT(logical_classification[index].proof != NULL);
		EXPECT(logical_classification[index].proof[0] != '\0');
	}
	EXPECT(tcti_target_ledger_validate(
		       logical_source, LOGICAL_SHIFT_ROWS,
		       logical_classification, LOGICAL_SHIFT_ROWS,
		       LOGICAL_SHIFT_ROWS, registry, registry_count,
		       &result) == 0);
	EXPECT(result.classified_rows == LOGICAL_SHIFT_ROWS);
	EXPECT(result.proof_gaps == 0);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "independent_array_counts_fail_before_indexing",
		  independent_array_counts_fail_before_indexing },
		{ "real_manifest_rows_pass_exact_binding",
		  real_manifest_rows_pass_exact_binding },
		{ "classification_order_does_not_change_binding",
		  classification_order_does_not_change_binding },
		{ "every_required_operation_maps_exactly_once",
		  every_required_operation_maps_exactly_once },
		{ "every_required_operation_has_explicit_requirements",
		  every_required_operation_has_explicit_requirements },
		{ "leaf_fingerprint_drift_fails", leaf_fingerprint_drift_fails },
		{ "extra_registry_binding_must_resolve_to_source",
		  extra_registry_binding_must_resolve_to_source },
		{ "alias_requires_source_bound_equivalence",
		  alias_requires_source_bound_equivalence },
		{ "stale_alias_canonical_fails_without_indexing_past_source",
		  stale_alias_canonical_fails_without_indexing_past_source },
		{ "exact_target_count_and_import_freshness_remain_enforced",
		  exact_target_count_and_import_freshness_remain_enforced },
		{ "logical_shifted_register_rows_pass_production_registry",
		  logical_shifted_register_rows_pass_production_registry },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run())
			return 1;
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
