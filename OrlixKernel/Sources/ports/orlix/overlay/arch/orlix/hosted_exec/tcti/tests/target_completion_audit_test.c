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
	EXPECT(result.classified_rows == 1087);
	EXPECT(result.unclassified_rows == 3263);
	EXPECT(result.required_el0_rows == 1076);
	EXPECT(result.non_el0_rows == 10);
	EXPECT(result.undefined_or_unallocated_rows == 1);
	EXPECT(result.alias_or_duplicate_rows == 0);
	EXPECT(result.absent_rows == 0);
	EXPECT(result.stale_rows == 0);
	EXPECT(result.source_bound_rows == 43);
	EXPECT(result.source_unbound_rows == 1044);
	EXPECT(result.invalid_relationship_rows == 0);
	EXPECT(result.invalid_source_rows == 0);
	EXPECT(result.invalid_registry_entries == 0);
	EXPECT(result.stale_proof_bindings == 196);
	EXPECT(result.unproved_obligation_bindings == 419);
	EXPECT(result.asl_availability_rows == 4350);
	EXPECT(result.invalid_asl_availability_rows == 0);
	EXPECT(result.unavailable_asl_rows == 4350);
	EXPECT(result.system_accessor_rows == 2014);
	EXPECT(result.mapped_system_accessor_rows == 2014);
	EXPECT(result.nonmapped_system_accessor_rows == 0);
	EXPECT(result.invalid_system_accessor_rows == 0);
	EXPECT(result.feature_field_domain_rows == 605);
	EXPECT(result.mapped_feature_field_domain_rows == 604);
	EXPECT(result.unresolved_feature_field_domain_rows == 604);
	EXPECT(result.ambiguous_feature_field_domain_rows == 1);
	EXPECT(result.invalid_feature_field_domain_rows == 0);
	EXPECT(result.runtime_capability_cohort_leaf_rows == 4350);
	EXPECT(result.runtime_capability_cohort_candidate_membership_rows == 5592);
	EXPECT(result.unresolved_runtime_capability_cohort_membership_rows == 5592);
	EXPECT(result.invalid_runtime_capability_cohort_rows == 0);
	EXPECT(result.errors == 19526);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_UNCLASSIFIED);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_UNPROVED_OBLIGATIONS);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_STALE_PROOF_BINDING);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT);
	return 0;
}

static int runtime_capability_cohorts_remain_explicitly_blocking(void)
{
	const struct tcti_runtime_capability_cohort_artifact *artifact;
	struct tcti_target_completion_result result = { 0 };

	artifact = tcti_runtime_capability_cohort_artifact_canonical();
	EXPECT(tcti_target_completion_validate_runtime_capability_cohorts(
		       artifact, &result) == -1);
	EXPECT(result.runtime_capability_cohort_leaf_rows == 4350);
	EXPECT(result.runtime_capability_cohort_candidate_membership_rows == 5592);
	EXPECT(result.unresolved_runtime_capability_cohort_membership_rows == 5592);
	EXPECT(result.invalid_runtime_capability_cohort_rows == 0);
	EXPECT(result.errors == 5592);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT);
	return 0;
}

static int mutated_runtime_capability_cohort_fails_before_counting(void)
{
	const struct tcti_runtime_capability_cohort_artifact *live;
	struct tcti_runtime_capability_cohort_artifact artifact;
	struct tcti_runtime_capability_cohort_membership *memberships;
	struct tcti_target_completion_result result = { 0 };

	live = tcti_runtime_capability_cohort_artifact_canonical();
	memberships = malloc(live->counts.membership_count * sizeof(*memberships));
	EXPECT(memberships != NULL);
	memcpy(memberships, live->memberships,
	       live->counts.membership_count * sizeof(*memberships));
	artifact = *live;
	artifact.memberships = memberships;
	memberships[0].source_offset++;
	EXPECT(tcti_target_completion_validate_runtime_capability_cohorts(
		       &artifact, &result) == -1);
	EXPECT(result.runtime_capability_cohort_leaf_rows == 0);
	EXPECT(result.runtime_capability_cohort_candidate_membership_rows == 0);
	EXPECT(result.unresolved_runtime_capability_cohort_membership_rows == 0);
	EXPECT(result.invalid_runtime_capability_cohort_rows == 1);
	EXPECT(result.errors == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT);
	free(memberships);
	return 0;
}

static int canonical_feature_field_domains_remain_explicitly_blocking(void)
{
	const struct tcti_feature_artifact *feature_artifact;
	const struct tcti_feature_field_domain_binding_artifact *artifact;
	struct tcti_target_completion_result result = { 0 };

	feature_artifact = tcti_feature_artifact_canonical();
	artifact = tcti_feature_field_domain_binding_artifact_canonical();
	EXPECT(tcti_target_completion_validate_feature_field_domains(
		       feature_artifact, artifact, &result) == -1);
	EXPECT(result.feature_field_domain_rows == 605);
	EXPECT(result.mapped_feature_field_domain_rows == 604);
	EXPECT(result.unresolved_feature_field_domain_rows == 604);
	EXPECT(result.ambiguous_feature_field_domain_rows == 1);
	EXPECT(result.invalid_feature_field_domain_rows == 0);
	EXPECT(result.errors == 605);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
	return 0;
}

static int mutated_feature_field_domain_artifact_fails_before_counting(void)
{
	const struct tcti_feature_artifact *feature_artifact;
	const struct tcti_feature_field_domain_binding_artifact *live;
	struct tcti_feature_field_domain_binding_artifact artifact;
	struct tcti_feature_field_domain_binding *bindings;
	struct tcti_target_completion_result result = { 0 };

	feature_artifact = tcti_feature_artifact_canonical();
	live = tcti_feature_field_domain_binding_artifact_canonical();
	bindings = malloc(live->occurrence_count * sizeof(*bindings));
	EXPECT(bindings != NULL);
	memcpy(bindings, live->bindings, live->occurrence_count * sizeof(*bindings));
	artifact = *live;
	artifact.bindings = bindings;
	/* Keep the aggregate identity current so the node provenance check owns it. */
	bindings[0].feature_source.offset++;
	artifact.identity = tcti_feature_field_domain_binding_identity(
		artifact.bindings, artifact.occurrence_count);
	EXPECT(tcti_target_completion_validate_feature_field_domains(
		       feature_artifact, &artifact, &result) == -1);
	EXPECT(result.feature_field_domain_rows == 0);
	EXPECT(result.invalid_feature_field_domain_rows == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
	free(bindings);

	artifact = *live;
	artifact.identity++;
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_feature_field_domains(
		       feature_artifact, &artifact, &result) == -1);
	EXPECT(result.feature_field_domain_rows == 0);
	EXPECT(result.invalid_feature_field_domain_rows == 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
	return 0;
}

static int canonical_system_accessors_are_supplemental(void)
{
	const struct tcti_target_completion_system_accessor_provenance *provenance;
	const struct tcti_target_completion_system_accessor_row *accessors;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_result result = { 0 };
	size_t accessor_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	accessors = tcti_target_completion_system_accessors(&accessor_count);
	provenance = tcti_target_completion_system_accessor_provenance();
	EXPECT(source_count == 4350);
	EXPECT(accessor_count == 2014);
	EXPECT(tcti_target_completion_validate_system_accessors(
		       source, source_count, provenance, accessors,
		       accessor_count, &result) == 0);
	EXPECT(result.system_accessor_rows == 2014);
	EXPECT(result.mapped_system_accessor_rows == 2014);
	EXPECT(result.nonmapped_system_accessor_rows == 0);
	EXPECT(result.invalid_system_accessor_rows == 0);
	EXPECT(!(result.error_mask &
		 TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR));
	return 0;
}

static int missing_and_drifted_system_accessors_fail_hard(void)
{
	const struct tcti_target_completion_system_accessor_provenance *provenance;
	const struct tcti_target_completion_system_accessor_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_system_accessor_provenance provenance_copy;
	struct tcti_target_completion_system_accessor_row *copy;
	struct tcti_target_completion_result result = { 0 };
	size_t accessor_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	live = tcti_target_completion_system_accessors(&accessor_count);
	provenance = tcti_target_completion_system_accessor_provenance();
	EXPECT(tcti_target_completion_validate_system_accessors(
		       source, source_count, provenance, live,
		       accessor_count - 1U, &result) == -1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);

	memset(&result, 0, sizeof(result));
	provenance_copy = *provenance;
	provenance_copy.source_sha256 =
		"6bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874";
	EXPECT(tcti_target_completion_validate_system_accessors(
		       source, source_count, &provenance_copy, live,
		       accessor_count, &result) == -1);
	EXPECT(result.invalid_system_accessor_rows != 0);

	copy = malloc(accessor_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, accessor_count * sizeof(*copy));
	copy[0].generic_leaf = "missing_direct_leaf";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_system_accessors(
		       source, source_count, provenance, copy,
		       accessor_count, &result) == -1);
	EXPECT(result.invalid_system_accessor_rows != 0);

	memcpy(copy, live, accessor_count * sizeof(*copy));
	copy[0].selector_identity ^= UINT64_C(1);
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_system_accessors(
		       source, source_count, provenance, copy,
		       accessor_count, &result) == -1);
	EXPECT(result.invalid_system_accessor_rows != 0);

	memcpy(copy, live, accessor_count * sizeof(*copy));
	copy[0].condition_identity ^= UINT64_C(1);
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_system_accessors(
		       source, source_count, provenance, copy,
		       accessor_count, &result) == -1);
	EXPECT(result.invalid_system_accessor_rows != 0);

	memcpy(copy, live, accessor_count * sizeof(*copy));
	copy[0].condition_source_length = 0U;
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_system_accessors(
		       source, source_count, provenance, copy,
		       accessor_count, &result) == -1);
	EXPECT(result.invalid_system_accessor_rows != 0);
	free(copy);
	return 0;
}

static int every_nonmapped_system_accessor_disposition_blocks(void)
{
	static const enum tcti_target_completion_system_accessor_disposition
	blocking[] = {
		TCTI_TARGET_COMPLETION_ACCESSOR_RESERVED,
		TCTI_TARGET_COMPLETION_ACCESSOR_PRIVILEGED,
		TCTI_TARGET_COMPLETION_ACCESSOR_UNSUPPORTED,
		TCTI_TARGET_COMPLETION_ACCESSOR_AMBIGUOUS,
		TCTI_TARGET_COMPLETION_ACCESSOR_CONTRADICTORY,
		TCTI_TARGET_COMPLETION_ACCESSOR_INVALID,
	};
	const struct tcti_target_completion_system_accessor_provenance *provenance;
	const struct tcti_target_completion_system_accessor_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_system_accessor_provenance provenance_copy;
	struct tcti_target_completion_system_accessor_row *copy;
	size_t accessor_count;
	size_t source_count;
	size_t index;

	source = tcti_target_completion_source(&source_count);
	live = tcti_target_completion_system_accessors(&accessor_count);
	provenance = tcti_target_completion_system_accessor_provenance();
	copy = malloc(accessor_count * sizeof(*copy));
	EXPECT(copy != NULL);
	for (index = 0; index < sizeof(blocking) / sizeof(blocking[0]);
	     index++) {
		struct tcti_target_completion_result result = { 0 };
		size_t *count;

		memcpy(copy, live, accessor_count * sizeof(*copy));
		provenance_copy = *provenance;
		provenance_copy.mapped_count--;
		copy[0].disposition = blocking[index];
		switch (blocking[index]) {
		case TCTI_TARGET_COMPLETION_ACCESSOR_RESERVED:
			count = &provenance_copy.reserved_count;
			break;
		case TCTI_TARGET_COMPLETION_ACCESSOR_PRIVILEGED:
			count = &provenance_copy.privileged_count;
			break;
		case TCTI_TARGET_COMPLETION_ACCESSOR_UNSUPPORTED:
			count = &provenance_copy.unsupported_count;
			break;
		case TCTI_TARGET_COMPLETION_ACCESSOR_AMBIGUOUS:
			count = &provenance_copy.ambiguous_count;
			break;
		case TCTI_TARGET_COMPLETION_ACCESSOR_CONTRADICTORY:
			count = &provenance_copy.contradictory_count;
			break;
		case TCTI_TARGET_COMPLETION_ACCESSOR_INVALID:
			count = &provenance_copy.invalid_count;
			break;
		case TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED:
		default:
			free(copy);
			return 1;
		}
		(*count)++;
		EXPECT(tcti_target_completion_validate_system_accessors(
			       source, source_count, &provenance_copy, copy,
			       accessor_count, &result) == -1);
		EXPECT(result.nonmapped_system_accessor_rows == 1);
		/* The checked source-derived identity rejects a forged disposition. */
		EXPECT(result.invalid_system_accessor_rows != 0);
		EXPECT(result.error_mask &
		       TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
	}
	free(copy);
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
	struct tcti_target_completion_result baseline;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;

	live = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	EXPECT(tcti_target_completion_audit(&baseline) == -1);
	copy = malloc(source_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, source_count * sizeof(*copy));
	copy[3434].pattern ^= 1U;
	EXPECT(tcti_target_completion_validate(
		       copy, source_count, classification,
		       classification_count, registry, registry_count,
		       &result) == -1);
	EXPECT(result.stale_proof_bindings ==
	       baseline.stale_proof_bindings + 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_STALE_PROOF_BINDING);
	EXPECT(result.source_bound_rows + 1 == baseline.source_bound_rows);
	EXPECT(result.source_unbound_rows == baseline.source_unbound_rows + 1);
	free(copy);
	return 0;
}

static int missing_source_binding_fails_hard(void)
{
	const struct tcti_target_completion_classification_row *live;
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_classification_row *copy;
	struct tcti_target_completion_result baseline;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t registry_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	live = tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	EXPECT(tcti_target_completion_audit(&baseline) == -1);
	EXPECT(live[3434].proof_id != NULL);
	EXPECT(live[3434].proof_id[0] != '\0');
	copy = malloc(classification_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, classification_count * sizeof(*copy));
	copy[3434].proof_id = "";
	EXPECT(tcti_target_completion_validate(
		       source, source_count, copy, classification_count,
		       registry, registry_count, &result) == -1);
	EXPECT(result.source_bound_rows + 1 == baseline.source_bound_rows);
	EXPECT(result.source_unbound_rows == baseline.source_unbound_rows + 1);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
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

static int one_byte_source_span_mutation_fails_hard(void)
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
	copy[3434].source_offset++;
	EXPECT(tcti_target_completion_validate(
		       copy, source_count, classification, classification_count,
		       registry, registry_count, &result) == -1);
	EXPECT(result.invalid_source_rows == 1);
	EXPECT(result.error_mask & TCTI_TARGET_COMPLETION_ERROR_SOURCE);
	free(copy);
	return 0;
}

static int one_byte_source_length_mutation_fails_hard(void)
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
	EXPECT(copy[3434].source_length > 1);
	copy[3434].source_length--;
	EXPECT(tcti_target_completion_validate(
		       copy, source_count, classification, classification_count,
		       registry, registry_count, &result) == -1);
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

static int one_byte_source_metadata_mutations_fail_hard(void)
{
	const struct tcti_target_completion_source_provenance *live;
	struct tcti_target_completion_source_provenance copy;
	struct tcti_target_completion_result result;

	live = tcti_target_completion_source_provenance();
	copy = *live;
	copy.source_byte_length++;
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_source_provenance(&copy,
							    &result) == -1);
	EXPECT(result.invalid_source_provenance == 1);
	copy = *live;
	copy.timestamp = "2026-06-24 17:12:15";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_source_provenance(&copy,
							    &result) == -1);
	EXPECT(result.invalid_source_provenance == 1);
	return 0;
}

static int canonical_asl_unavailability_blocks_semantic_completion(void)
{
	struct tcti_target_completion_result result;

	EXPECT(tcti_target_completion_audit(&result) == -1);
	EXPECT(result.asl_availability_rows == 4350);
	EXPECT(result.invalid_asl_availability_rows == 0);
	/* The pinned package has operation references but no shared ASL corpus. */
	EXPECT(result.unavailable_asl_rows == 4350);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	return 0;
}

static int malformed_asl_row_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *provenance;
	const struct tcti_target_completion_asl_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_asl_row *copy;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	provenance = tcti_target_completion_asl_provenance();
	live = tcti_target_completion_asl_availability(&availability_count);
	copy = malloc(availability_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, availability_count * sizeof(*copy));
	copy[3434].source_offset = UINT32_MAX;
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, provenance, copy, availability_count,
		       &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 1);
	EXPECT(result.asl_availability_rows == 4349);
	EXPECT(result.unavailable_asl_rows == 4349);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	free(copy);
	return 0;
}

static int malformed_asl_operational_note_provenance_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *provenance;
	const struct tcti_target_completion_asl_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_asl_row *copy;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	provenance = tcti_target_completion_asl_provenance();
	live = tcti_target_completion_asl_availability(&availability_count);
	copy = malloc(availability_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, availability_count * sizeof(*copy));
	copy[3434].operational_note_presence = "present";
	copy[3434].operational_note_source_length = 1U;
	copy[3434].operational_note_sha256 = "not-a-digest";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, provenance, copy, availability_count,
		       &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 1);
	EXPECT(result.asl_availability_rows == 4349);
	EXPECT(result.unavailable_asl_rows == 4349);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	free(copy);
	return 0;
}

static int absent_asl_operational_note_span_drift_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *provenance;
	const struct tcti_target_completion_asl_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_asl_row *copy;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	provenance = tcti_target_completion_asl_provenance();
	live = tcti_target_completion_asl_availability(&availability_count);
	copy = malloc(availability_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, availability_count * sizeof(*copy));
	copy[3434].operational_note_source_offset = 1U;
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, provenance, copy, availability_count,
		       &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 1);
	EXPECT(result.asl_availability_rows == 4349);
	EXPECT(result.unavailable_asl_rows == 4349);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	free(copy);
	return 0;
}

static int missing_asl_row_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *provenance;
	const struct tcti_target_completion_asl_row *availability;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	provenance = tcti_target_completion_asl_provenance();
	availability = tcti_target_completion_asl_availability(&availability_count);
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, provenance, availability,
		       availability_count - 1, &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 1);
	EXPECT(result.asl_availability_rows == 4349);
	EXPECT(result.unavailable_asl_rows == 4349);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	return 0;
}

static int mismatched_asl_source_tuple_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *provenance;
	const struct tcti_target_completion_asl_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_asl_row *copy;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	provenance = tcti_target_completion_asl_provenance();
	live = tcti_target_completion_asl_availability(&availability_count);
	copy = malloc(availability_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, availability_count * sizeof(*copy));
	copy[3434].operation_id = "tampered_operation";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, provenance, copy, availability_count,
		       &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 1);
	EXPECT(result.asl_availability_rows == 4349);
	EXPECT(result.unavailable_asl_rows == 4349);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	free(copy);
	return 0;
}

static int mismatched_asl_operation_object_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *provenance;
	const struct tcti_target_completion_asl_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_asl_row *copy;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	provenance = tcti_target_completion_asl_provenance();
	live = tcti_target_completion_asl_availability(&availability_count);
	copy = malloc(availability_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, availability_count * sizeof(*copy));
	copy[3434].operation_object = "operations/not_the_source_operation";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, provenance, copy, availability_count,
		       &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 1);
	EXPECT(result.asl_availability_rows == 4349);
	EXPECT(result.unavailable_asl_rows == 4349);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	free(copy);
	return 0;
}

static int malformed_asl_provenance_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *live;
	const struct tcti_target_completion_asl_row *availability;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_asl_provenance copy;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	live = tcti_target_completion_asl_provenance();
	availability = tcti_target_completion_asl_availability(&availability_count);
	copy = *live;
	copy.source_sha256 = "incorrect";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, &copy, availability, availability_count,
		       &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 0);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	return 0;
}

static int unproven_available_asl_status_fails_hard(void)
{
	const struct tcti_target_completion_asl_provenance *provenance;
	const struct tcti_target_completion_asl_row *live;
	const struct tcti_target_completion_source_row *source;
	struct tcti_target_completion_asl_row *copy;
	struct tcti_target_completion_result result;
	size_t availability_count;
	size_t source_count;

	source = tcti_target_completion_source(&source_count);
	provenance = tcti_target_completion_asl_provenance();
	live = tcti_target_completion_asl_availability(&availability_count);
	copy = malloc(availability_count * sizeof(*copy));
	EXPECT(copy != NULL);
	memcpy(copy, live, availability_count * sizeof(*copy));
	/* A future present corpus needs a new schema with body provenance. */
	copy[3434].availability = "shared_asl_available";
	memset(&result, 0, sizeof(result));
	EXPECT(tcti_target_completion_validate_asl_availability(
		       source, source_count, provenance, copy, availability_count,
		       &result) == -1);
	EXPECT(result.invalid_asl_availability_rows == 1);
	EXPECT(result.asl_availability_rows == 4349);
	EXPECT(result.unavailable_asl_rows == 4349);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
	free(copy);
	return 0;
}

static int alias_relationship_does_not_require_source_identity(void)
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
	EXPECT(result.invalid_relationship_rows == 0);
	EXPECT(result.source_unbound_rows > 0);
	EXPECT(result.error_mask &
	       TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
	free(binding_copy);
	free(registry_copy);
	free(copy);
	return 0;
}

static int obligation_projection_rejects_noncanonical_capacity(void)
{
	struct tcti_target_completion_obligation obligation = {
		.ordinal = UINT32_MAX,
	};
	struct tcti_target_completion_result result;

	EXPECT(tcti_target_completion_audit_with_obligations(
		       &result, &obligation,
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS - 1U) == -1);
	EXPECT(obligation.ordinal == UINT32_MAX);
	EXPECT(tcti_target_completion_audit_with_obligations(&result, NULL, 1) ==
	       -1);
	return 0;
}

static int obligation_projection_matches_canonical_red_audit(void)
{
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_completion_source_row *source;
	const struct tcti_runtime_capability_cohort_artifact *runtime;
	struct tcti_target_completion_obligation *obligations;
	struct tcti_target_completion_result result;
	size_t classification_count;
	size_t source_count;
	size_t absent_asl = 0;
	size_t missing_configuration = 0;
	size_t missing_operand = 0;
	size_t incomplete_union = 0;
	size_t unresolved_feature = 0;
	size_t runtime_memberships = 0;
	size_t unclassified = 0;
	size_t required = 0;
	size_t non_el0 = 0;
	size_t undefined = 0;
	size_t aliases = 0;
	size_t index;

	source = tcti_target_completion_source(&source_count);
	runtime = tcti_runtime_capability_cohort_artifact_canonical();
	classification =
		tcti_target_completion_classification(&classification_count);
	obligations = calloc(TCTI_TARGET_COMPLETION_SOURCE_ROWS,
			    sizeof(*obligations));
	EXPECT(obligations != NULL);
	EXPECT(tcti_target_completion_audit_with_obligations(
		       &result, obligations,
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS) == -1);
	EXPECT(source_count == TCTI_TARGET_COMPLETION_SOURCE_ROWS);
	EXPECT(runtime != NULL);
	EXPECT(runtime->counts.leaf_count == source_count);
	EXPECT(classification_count == TCTI_TARGET_COMPLETION_SOURCE_ROWS);
	for (index = 0; index < TCTI_TARGET_COMPLETION_SOURCE_ROWS; index++) {
		EXPECT(obligations[index].ordinal == source[index].ordinal);
		EXPECT(!strcmp(obligations[index].name, source[index].name));
		EXPECT(!strcmp(obligations[index].operation_id,
			       source[index].operation_id));
		EXPECT(obligations[index].classification ==
		       classification[index].classification);
		EXPECT(obligations[index].asl_state ==
		       TCTI_TARGET_COMPLETION_ASL_ABSENT_BLOCKING);
		EXPECT(obligations[index].blocker_mask &
		       TCTI_TARGET_COMPLETION_BLOCKER_ASL);
		if (obligations[index].asl_state ==
		    TCTI_TARGET_COMPLETION_ASL_ABSENT_BLOCKING)
			absent_asl++;
		if (obligations[index].blocker_mask &
		    TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION)
			unresolved_feature++;
		if (obligations[index].feature_union_state ==
		    TCTI_TARGET_COMPLETION_FEATURE_UNION_MISSING_CONFIGURATION)
			missing_configuration++;
		if (obligations[index].feature_union_state ==
		    TCTI_TARGET_COMPLETION_FEATURE_UNION_MISSING_OPERAND)
			missing_operand++;
		if (obligations[index].known_feature_union_reason_mask &
		    TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INCOMPLETE)
			incomplete_union++;
		runtime_memberships += obligations[index].runtime_candidate_count;
		EXPECT(obligations[index].runtime_candidate_first ==
		       runtime->leaves[index].first_membership);
		EXPECT(obligations[index].runtime_candidate_count ==
		       runtime->leaves[index].membership_count);
		if (classification[index].classification ==
		    TCTI_TARGET_COMPLETION_UNCLASSIFIED)
			unclassified++;
		switch (obligations[index].classification) {
		case TCTI_TARGET_COMPLETION_REQUIRED_EL0:
			required++;
			break;
		case TCTI_TARGET_COMPLETION_NON_EL0:
			non_el0++;
			break;
		case TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED:
			undefined++;
			break;
		case TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE:
			aliases++;
			break;
		case TCTI_TARGET_COMPLETION_UNCLASSIFIED:
			break;
		}
	}
	EXPECT(unclassified == result.unclassified_rows);
	EXPECT(required == result.required_el0_rows);
	EXPECT(non_el0 == result.non_el0_rows);
	EXPECT(undefined == result.undefined_or_unallocated_rows);
	EXPECT(aliases == result.alias_or_duplicate_rows);
	EXPECT(absent_asl == result.unavailable_asl_rows);
	EXPECT(unresolved_feature == result.unresolved_feature_applicability_rows);
	EXPECT(missing_configuration ==
	       result.unresolved_feature_configuration_rows);
	EXPECT(missing_operand == result.unresolved_instruction_operand_rows);
	EXPECT(incomplete_union == TCTI_TARGET_COMPLETION_SOURCE_ROWS);
	EXPECT(runtime_memberships ==
	       result.runtime_capability_cohort_candidate_membership_rows);
	/* Ordinal zero is a feature-conditioned, still-unclassified SVE leaf. */
	EXPECT(obligations[0].classification ==
	       TCTI_TARGET_COMPLETION_UNCLASSIFIED);
	EXPECT(obligations[0].blocker_mask &
	       TCTI_TARGET_COMPLETION_BLOCKER_UNCLASSIFIED);
	EXPECT(obligations[0].feature_union_state ==
	       TCTI_TARGET_COMPLETION_FEATURE_UNION_MISSING_CONFIGURATION);
	EXPECT(obligations[0].blocker_mask &
	       TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION);
	EXPECT(obligations[0].proof_state == TCTI_TARGET_COMPLETION_PROOF_NONE);
	EXPECT(obligations[0].runtime_candidate_state ==
	       TCTI_TARGET_COMPLETION_RUNTIME_CANDIDATE_UNRESOLVED);
	/* A real registry binding remains red until native execution discharges it. */
	EXPECT(obligations[2300].classification ==
	       TCTI_TARGET_COMPLETION_NON_EL0);
	EXPECT(!strcmp(obligations[2300].proof_id,
	       "kunit:source-leaf-ereta-non-el0"));
	EXPECT(obligations[2300].proof_state ==
	       TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_UNPROVED);
	EXPECT(obligations[2300].unproved_obligations != 0U);
	EXPECT(obligations[2300].blocker_mask &
	       TCTI_TARGET_COMPLETION_BLOCKER_UNPROVED_OBLIGATIONS);
	EXPECT(obligations[2300].blocker_mask &
	       TCTI_TARGET_COMPLETION_BLOCKER_EXECUTION_EVIDENCE);
	free(obligations);
	return 0;
}

static int obligation_projection_retains_exact_ereta_delta(void)
{
	struct tcti_target_completion_obligation *obligations;
	struct tcti_target_completion_result result;
	size_t non_el0 = 0;
	size_t source_bound = 0;
	size_t unproved_source_bound_rows = 0;
	size_t index;

	obligations = calloc(TCTI_TARGET_COMPLETION_SOURCE_ROWS,
			    sizeof(*obligations));
	EXPECT(obligations != NULL);
	EXPECT(tcti_target_completion_audit_with_obligations(
		       &result, obligations,
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS) == -1);
	for (index = 0; index < TCTI_TARGET_COMPLETION_SOURCE_ROWS; index++) {
		if (obligations[index].classification ==
		    TCTI_TARGET_COMPLETION_NON_EL0)
			non_el0++;
		if (obligations[index].proof_state ==
		    TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_UNPROVED ||
		    obligations[index].proof_state ==
		    TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_NO_UNPROVED_METADATA)
			source_bound++;
		if (obligations[index].unproved_obligations)
			unproved_source_bound_rows++;
	}
	EXPECT(non_el0 == result.non_el0_rows);
	EXPECT(source_bound == result.source_bound_rows);
	/*
	 * The projection has one row per source leaf and only exposes proof
	 * metadata selected by that leaf's classification.  The aggregate counts
	 * every registry binding, including still-unclassified source bindings,
	 * so these values intentionally have different units.
	 */
	EXPECT(unproved_source_bound_rows == source_bound);
	EXPECT(unproved_source_bound_rows <=
	       result.unproved_obligation_bindings);
	for (index = 2300; index <= 2301; index++) {
		EXPECT(obligations[index].classification ==
		       TCTI_TARGET_COMPLETION_NON_EL0);
		EXPECT(obligations[index].proof_state ==
		       TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_UNPROVED);
		EXPECT(obligations[index].blocker_mask &
		       TCTI_TARGET_COMPLETION_BLOCKER_UNPROVED_OBLIGATIONS);
		EXPECT(obligations[index].blocker_mask &
		       TCTI_TARGET_COMPLETION_BLOCKER_EXECUTION_EVIDENCE);
	}
	EXPECT(result.non_el0_rows == 10U);
	EXPECT(result.source_bound_rows == 43U);
	EXPECT(result.unproved_obligation_bindings == 419U);
	free(obligations);
	return 0;
}

static int malformed_projection_dependencies_leave_output_untouched(void)
{
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_completion_source_row *source_copy;
	struct tcti_target_completion_classification_row *classification_copy;
	struct tcti_target_proof_registry_entry *registry_copy;
	struct tcti_target_completion_audit_inputs_for_test inputs;
	struct tcti_target_completion_obligation *before;
	struct tcti_target_completion_obligation *obligations;
	struct tcti_target_completion_result result;
	size_t source_count;
	size_t classification_count;
	size_t registry_count;
	size_t classification_index;
	size_t bytes = TCTI_TARGET_COMPLETION_SOURCE_ROWS * sizeof(*obligations);

	source = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	source_copy = malloc(source_count * sizeof(*source_copy));
	classification_copy = malloc(classification_count * sizeof(*classification_copy));
	registry_copy = malloc(registry_count * sizeof(*registry_copy));
	before = malloc(bytes);
	obligations = malloc(bytes);
	EXPECT(source_copy != NULL);
	EXPECT(classification_copy != NULL);
	EXPECT(registry_copy != NULL);
	EXPECT(before != NULL);
	EXPECT(obligations != NULL);
	memcpy(source_copy, source, source_count * sizeof(*source_copy));
	memcpy(classification_copy, classification,
	       classification_count * sizeof(*classification_copy));
	memcpy(registry_copy, registry, registry_count * sizeof(*registry_copy));
	memset(obligations, 0xa5, bytes);
	memcpy(before, obligations, bytes);
	inputs = (struct tcti_target_completion_audit_inputs_for_test) {
		.source = source_copy,
		.source_count = source_count,
		.classification = classification_copy,
		.classification_count = classification_count,
		.registry = registry_copy,
		.registry_count = registry_count,
	};

	source_copy[0].name = NULL;
	EXPECT(tcti_target_completion_audit_with_inputs_for_test(
		       &inputs, &result, obligations,
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS) == -1);
	EXPECT(result.invalid_source_rows != 0U);
	EXPECT(!memcmp(obligations, before, bytes));
	source_copy[0] = source[0];

	for (classification_index = 0;
	     classification_index < classification_count;
	     classification_index++)
		if (classification_copy[classification_index].classification ==
		    TCTI_TARGET_COMPLETION_REQUIRED_EL0)
			break;
	EXPECT(classification_index < classification_count);
	classification_copy[classification_index].relation =
		TCTI_TARGET_COMPLETION_RELATION_ALIAS;
	classification_copy[classification_index].canonical_name =
		classification_copy[classification_index].name;
	EXPECT(tcti_target_completion_audit_with_inputs_for_test(
		       &inputs, &result, obligations,
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS) == -1);
	EXPECT(result.invalid_relationship_rows != 0U);
	EXPECT(!memcmp(obligations, before, bytes));
	classification_copy[classification_index] =
		classification[classification_index];

	registry_copy[0].id = "";
	EXPECT(tcti_target_completion_audit_with_inputs_for_test(
		       &inputs, &result, obligations,
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS) == -1);
	EXPECT(result.invalid_registry_entries != 0U);
	EXPECT(!memcmp(obligations, before, bytes));
	free(obligations);
	free(before);
	free(registry_copy);
	free(classification_copy);
	free(source_copy);
	return 0;
}

static int static_proof_metadata_never_becomes_execution_evidence(void)
{
	const struct tcti_target_completion_source_row *source;
	const struct tcti_target_completion_classification_row *classification;
	const struct tcti_target_proof_registry_entry *registry;
	struct tcti_target_proof_registry_entry *registry_copy;
	struct tcti_target_completion_audit_inputs_for_test inputs;
	struct tcti_target_completion_obligation *obligations;
	struct tcti_target_completion_result result;
	size_t source_count;
	size_t classification_count;
	size_t registry_count;
	size_t index;

	source = tcti_target_completion_source(&source_count);
	classification =
		tcti_target_completion_classification(&classification_count);
	registry = tcti_target_proof_registry_entries(&registry_count);
	registry_copy = malloc(registry_count * sizeof(*registry_copy));
	obligations = calloc(TCTI_TARGET_COMPLETION_SOURCE_ROWS,
			    sizeof(*obligations));
	EXPECT(registry_copy != NULL);
	EXPECT(obligations != NULL);
	memcpy(registry_copy, registry, registry_count * sizeof(*registry_copy));
	for (index = 0; index < registry_count; index++)
		if (!strcmp(registry_copy[index].id,
			    "kunit:source-leaf-ereta-non-el0"))
			break;
	EXPECT(index < registry_count);
	inputs = (struct tcti_target_completion_audit_inputs_for_test) {
		.source = source,
		.source_count = source_count,
		.classification = classification,
		.classification_count = classification_count,
		.registry = registry_copy,
		.registry_count = registry_count,
	};
	EXPECT(tcti_target_completion_audit_with_inputs_for_test(
		       &inputs, &result, obligations,
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS) == -1);
	EXPECT(obligations[2300].proof_state ==
	       TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_UNPROVED);
	EXPECT(obligations[2300].blocker_mask &
	       TCTI_TARGET_COMPLETION_BLOCKER_EXECUTION_EVIDENCE);
	EXPECT(obligations[2300].blocker_mask &
	       TCTI_TARGET_COMPLETION_BLOCKER_UNPROVED_OBLIGATIONS);
	free(obligations);
	free(registry_copy);
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
		{ "runtime_capability_cohorts_remain_explicitly_blocking",
		  runtime_capability_cohorts_remain_explicitly_blocking },
		{ "mutated_runtime_capability_cohort_fails_before_counting",
		  mutated_runtime_capability_cohort_fails_before_counting },
		{ "canonical_feature_field_domains_remain_explicitly_blocking",
		  canonical_feature_field_domains_remain_explicitly_blocking },
		{ "mutated_feature_field_domain_artifact_fails_before_counting",
		  mutated_feature_field_domain_artifact_fails_before_counting },
		{ "canonical_system_accessors_are_supplemental",
		  canonical_system_accessors_are_supplemental },
		{ "missing_and_drifted_system_accessors_fail_hard",
		  missing_and_drifted_system_accessors_fail_hard },
		{ "every_nonmapped_system_accessor_disposition_blocks",
		  every_nonmapped_system_accessor_disposition_blocks },
		{ "absent_and_stale_rows_fail_hard",
		  absent_and_stale_rows_fail_hard },
		{ "invalid_relationship_fails_hard",
		  invalid_relationship_fails_hard },
		{ "wrong_counts_fail_hard", wrong_counts_fail_hard },
		{ "stale_proof_binding_fails_hard",
		  stale_proof_binding_fails_hard },
		{ "missing_source_binding_fails_hard",
		  missing_source_binding_fails_hard },
		{ "malformed_inputs_fail_without_crashing",
		  malformed_inputs_fail_without_crashing },
		{ "malformed_source_name_never_reaches_strcmp",
		  malformed_source_name_never_reaches_strcmp },
		{ "one_byte_source_span_mutation_fails_hard",
		  one_byte_source_span_mutation_fails_hard },
		{ "one_byte_source_length_mutation_fails_hard",
		  one_byte_source_length_mutation_fails_hard },
		{ "invalid_registry_is_never_consumed_after_validation",
		  invalid_registry_is_never_consumed_after_validation },
		{ "malformed_source_provenance_fails_hard",
		  malformed_source_provenance_fails_hard },
		{ "one_byte_source_metadata_mutations_fail_hard",
		  one_byte_source_metadata_mutations_fail_hard },
		{ "canonical_asl_unavailability_blocks_semantic_completion",
		  canonical_asl_unavailability_blocks_semantic_completion },
		{ "malformed_asl_row_fails_hard", malformed_asl_row_fails_hard },
		{ "malformed_asl_operational_note_provenance_fails_hard",
		  malformed_asl_operational_note_provenance_fails_hard },
		{ "absent_asl_operational_note_span_drift_fails_hard",
		  absent_asl_operational_note_span_drift_fails_hard },
		{ "missing_asl_row_fails_hard", missing_asl_row_fails_hard },
		{ "mismatched_asl_source_tuple_fails_hard",
		  mismatched_asl_source_tuple_fails_hard },
		{ "mismatched_asl_operation_object_fails_hard",
		  mismatched_asl_operation_object_fails_hard },
		{ "malformed_asl_provenance_fails_hard",
		  malformed_asl_provenance_fails_hard },
		{ "unproven_available_asl_status_fails_hard",
		  unproven_available_asl_status_fails_hard },
		{ "alias_relationship_does_not_require_source_identity",
		  alias_relationship_does_not_require_source_identity },
		{ "obligation_projection_rejects_noncanonical_capacity",
		  obligation_projection_rejects_noncanonical_capacity },
		{ "obligation_projection_matches_canonical_red_audit",
		  obligation_projection_matches_canonical_red_audit },
		{ "obligation_projection_retains_exact_ereta_delta",
		  obligation_projection_retains_exact_ereta_delta },
		{ "malformed_projection_dependencies_leave_output_untouched",
		  malformed_projection_dependencies_leave_output_untouched },
		{ "static_proof_metadata_never_becomes_execution_evidence",
		  static_proof_metadata_never_becomes_execution_evidence },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run())
			return 1;
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
