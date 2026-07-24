/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_completion_audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: EXPECT(%s) failed\n", \
				__FILE__, __LINE__, #condition); \
			return 1; \
		} \
	} while (0)

static int current_inventory_fails_with_exact_incomplete_counts(void)
{
	struct tcti_target_completion_result result;

	EXPECT(tcti_target_completion_audit(&result) == -1);
	EXPECT(result.source_rows == 4350);
	EXPECT(result.classification_rows == 4350);
	EXPECT(result.classified_rows == 1078);
	EXPECT(result.unclassified_rows == 3272);
	EXPECT(result.required_el0_rows == 1068);
	EXPECT(result.non_el0_rows == 9);
	EXPECT(result.undefined_or_unallocated_rows == 1);
	EXPECT(result.alias_or_duplicate_rows == 0);
	EXPECT(result.absent_rows == 0);
	EXPECT(result.stale_rows == 0);
	EXPECT(result.proved_rows == 16);
	EXPECT(result.unproved_rows == 1062);
	EXPECT(result.invalid_relationship_rows == 0);
	EXPECT(result.invalid_source_rows == 0);
	EXPECT(result.invalid_registry_entries == 0);
	EXPECT(result.stale_proof_bindings == 0);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_UNCLASSIFIED);
	EXPECT(result.error_mask & TCTI_TARGET_COMPLETION_ERROR_UNPROVED);
	return 0;
}

static int absent_and_stale_rows_fail_hard(void)
{
	const struct tcti_target_completion_classification_row *live;
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_classification_row *copy;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	live = tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	copy = malloc(classification_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, classification_count * sizeof(*copy));
	copy[0].name = "stale_inventory_leaf";
	EXPECT(tcti_target_completion_validate(
		       source, source_count, copy, classification_count,
		       registry, registry_count, &result) == -1);
	EXPECT(result.absent_rows == 1);
	EXPECT(result.stale_rows == 1);
	EXPECT(result.error_mask & TCTI_TARGET_COMPLETION_ERROR_ABSENT);
	EXPECT(result.error_mask & TCTI_TARGET_COMPLETION_ERROR_STALE);
	free(copy);
	return 0;
}

static int invalid_relationship_fails_hard(void)
{
	const struct tcti_target_completion_classification_row *live;
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_classification_row *copy;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;
	size_t index;

	source = tcti_target_completion_source(&source_count);
	live = tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	copy = malloc(classification_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, classification_count * sizeof(*copy));
	for (index = 0; index < classification_count; index++)
		if (copy[index].classification ==
		    TCTI_TARGET_COMPLETION_REQUIRED_EL0)
			break;
	EXPECT(index < classification_count);
	copy[index].relation = TCTI_TARGET_COMPLETION_RELATION_ALIAS;
	copy[index].canonical_name = copy[index].name;
	EXPECT(tcti_target_completion_validate(
		       source, source_count, copy, classification_count,
		       registry, registry_count, &result) == -1);
	EXPECT(result.invalid_relationship_rows == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP);
	free(copy);
	return 0;
}

static int wrong_counts_fail_hard(void)
{
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	EXPECT(tcti_target_completion_validate(
		       source, source_count - 1, classification,
		       classification_count, registry, registry_count,
		       &result) == -1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_SOURCE_COUNT);
	EXPECT(tcti_target_completion_validate(
		       source, source_count, classification,
		       classification_count - 1, registry, registry_count,
		       &result) == -1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_CLASSIFICATION_COUNT);
	return 0;
}

static int stale_proof_binding_fails_hard(void)
{
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_completion_source_row *live;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_source_row *copy;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;

	live = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	copy = malloc(source_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, source_count * sizeof(*copy));
	copy[3434].pattern ^= 1U;
	EXPECT(tcti_target_completion_validate(
		       copy, source_count, classification,
		       classification_count, registry, registry_count,
		       &result) == -1);
	EXPECT(result.stale_proof_bindings == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_STALE_PROOF_BINDING);
	EXPECT(result.proved_rows == 15);
	EXPECT(result.unproved_rows == 1063);
	free(copy);
	return 0;
}

static int malformed_inputs_fail_without_crashing(void)
{
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_completion_source_row *live;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_source_row *copy;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;

	live = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	copy = malloc(source_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, source_count * sizeof(*copy));
	copy[3434].mnemonic = NULL;
	EXPECT(tcti_target_completion_validate(
		       copy, source_count, classification,
		       classification_count, registry, registry_count, &result) == -1);
	EXPECT(result.invalid_source_rows == 1);
	EXPECT(result.error_mask & TCTI_TARGET_COMPLETION_ERROR_SOURCE);
	free(copy);
	return 0;
}

static int malformed_source_name_never_reaches_strcmp(void)
{
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_completion_source_row *live;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_source_row *copy;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;

	live = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	copy = malloc(source_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, source_count * sizeof(*copy));
	/* The next valid row exercises the old duplicate-name strcmp path. */
	copy[0].name = NULL;
	EXPECT(tcti_target_completion_validate(
		       copy, source_count, classification,
		       classification_count, registry, registry_count, &result) == -1);
	EXPECT(result.invalid_source_rows == 1);
	EXPECT(result.error_mask & TCTI_TARGET_COMPLETION_ERROR_SOURCE);
	free(copy);
	return 0;
}

static int invalid_registry_is_never_consumed_after_validation(void)
{
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_proof_registry_entry poison = {
		.id = "invalid-registry",
		.operation_id = "invalid",
		/* Validation fails before it can inspect these poisoned members. */
		.bindings = (const struct tcti_target_proof_binding *)(uintptr_t)1,
		.binding_count = 1,
		.kunit_cases = (const struct tcti_target_proof_case *)(uintptr_t)1,
		.kunit_case_count = 1,
	};
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	EXPECT(tcti_target_completion_validate(
		       source, source_count, classification, classification_count,
		       &poison, 1, &result) == -1);
	EXPECT(result.invalid_registry_entries == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_PROOF_REGISTRY);
	return 0;
}

static int malformed_source_provenance_fails_hard(void)
{
	const struct tcti_target_completion_source_provenance *live;
	struct tcti_target_completion_source_provenance copy;
	struct tcti_target_completion_result result;

	live = tcti_target_completion_source_provenance();
	copy = *live;
	copy.source_sha256 = "incorrect";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_source_provenance(&copy,
							    &result) == -1);
	EXPECT(result.invalid_source_provenance == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_SOURCE_PROVENANCE);
	return 0;
}

static int fabricated_alias_cannot_inherit_canonical_proof(void)
{
	const struct tcti_target_completion_classification_row *live;
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_proof_registry_entry *registry;
	const struct tcti_target_proof_registry_entry *canonical_entry;
	struct tcti_target_completion_classification_row *copy;
	struct tcti_target_proof_binding *binding_copy;
	struct tcti_target_proof_registry_entry *registry_copy;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t entry_index;
	size_t registry_count;
	size_t source_count;
	size_t index;

	source = tcti_target_completion_source(&source_count);
	live = tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	copy = malloc(classification_count * sizeof(*copy));
	registry_copy = malloc(registry_count * sizeof(*registry_copy));
	EXPECT(copy != NULL);
	EXPECT(registry_copy != NULL);
	memcpy(copy, live, classification_count * sizeof(*copy));
	memcpy(registry_copy, registry, registry_count * sizeof(*registry_copy));
	for (index = 0; index < source_count; index++)
		if (index != 3434 &&
		    copy[index].classification ==
			    TCTI_TARGET_COMPLETION_REQUIRED_EL0 &&
		    !strcmp(source[index].condition_tcnd_hex,
			    source[3434].condition_tcnd_hex) &&
		    strcmp(source[index].operation_id,
			   source[3434].operation_id))
			break;
	EXPECT(index < source_count);
	copy[index].classification =
		TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE;
	copy[index].relation = TCTI_TARGET_COMPLETION_RELATION_ALIAS;
	copy[index].canonical_name = source[3434].name;
	copy[index].evidence = "fabricated-alias";
	copy[index].proof_id = live[3434].proof_id;
	for (entry_index = 0; entry_index < registry_count; entry_index++)
		if (!strcmp(registry_copy[entry_index].id, copy[index].proof_id))
			break;
	EXPECT(entry_index < registry_count);
	canonical_entry = &registry_copy[entry_index];
	binding_copy = malloc((canonical_entry->binding_count + 1) *
			      sizeof(*binding_copy));
	EXPECT(binding_copy != NULL);
	memcpy(binding_copy, canonical_entry->bindings,
	       canonical_entry->binding_count * sizeof(*binding_copy));
	binding_copy[canonical_entry->binding_count] =
		(const struct tcti_target_proof_binding) {
			.leaf_name = source[index].name,
			.mnemonic = source[index].mnemonic,
			.encoding_mask = source[index].mask,
			.encoding_pattern = source[index].pattern,
			.condition_tcnd_hex = source[index].condition_tcnd_hex,
			.kunit_case_mask = canonical_entry->bindings[0].kunit_case_mask,
		};
	registry_copy[entry_index].bindings = binding_copy;
	registry_copy[entry_index].binding_count++;
	EXPECT(tcti_target_completion_validate(
		       source, source_count, copy, classification_count,
		       registry_copy, registry_count, &result) == -1);
	EXPECT(result.invalid_relationship_rows == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP);
	free(binding_copy);
	free(registry_copy);
	free(copy);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "current_inventory_fails_with_exact_incomplete_counts",
		  current_inventory_fails_with_exact_incomplete_counts },
		{ "absent_and_stale_rows_fail_hard",
		  absent_and_stale_rows_fail_hard },
		{ "invalid_relationship_fails_hard",
		  invalid_relationship_fails_hard },
		{ "wrong_counts_fail_hard", wrong_counts_fail_hard },
		{ "stale_proof_binding_fails_hard",
		  stale_proof_binding_fails_hard },
		{ "malformed_inputs_fail_without_crashing",
		  malformed_inputs_fail_without_crashing },
		{ "malformed_source_name_never_reaches_strcmp",
		  malformed_source_name_never_reaches_strcmp },
		{ "invalid_registry_is_never_consumed_after_validation",
		  invalid_registry_is_never_consumed_after_validation },
		{ "malformed_source_provenance_fails_hard",
		  malformed_source_provenance_fails_hard },
		{ "fabricated_alias_cannot_inherit_canonical_proof",
		  fabricated_alias_cannot_inherit_canonical_proof },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run())
			return 1;
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
