/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_completion_audit.h"
#include "target_feature_artifact.h"
#include "target_feature_domain.h"
#include "target_instruction_artifact.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define stringify_1(value) #value
#define stringify(value) stringify_1(value)

#define UNCLASSIFIED ORLIX_TCTI_TARGET_COMPLETION_UNCLASSIFIED
#define REQUIRED ORLIX_TCTI_TARGET_COMPLETION_REQUIRED_EL0
#define REQUIRED_EL0 ORLIX_TCTI_TARGET_COMPLETION_REQUIRED_EL0
#define NON_EL0 ORLIX_TCTI_TARGET_COMPLETION_NON_EL0
#define ARCHITECTURALLY_UNDEFINED \
	ORLIX_TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED
#define ARCH_UNDEFINED_OR_UNALLOCATED \
	ORLIX_TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED
#define ALIAS_OR_DUPLICATE ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE
#define ORLIX_TCTI_A64_TARGET_RELATION_NONE ORLIX_TCTI_TARGET_COMPLETION_RELATION_NONE
#define ORLIX_TCTI_A64_TARGET_RELATION_ALIAS ORLIX_TCTI_TARGET_COMPLETION_RELATION_ALIAS
#define ORLIX_TCTI_A64_TARGET_RELATION_DUPLICATE \
	ORLIX_TCTI_TARGET_COMPLETION_RELATION_DUPLICATE

static const struct orlix_tcti_target_completion_source_provenance source_provenance = {
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(arch_value, build_value, release_value, \
					schema_value, sha_value, count_value, \
					timestamp_value, source_length_value) \
	arch_value, build_value, release_value, schema_value, timestamp_value, \
	sha_value, source_length_value, count_value
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(...)
#include "../isa/source_manifest.def"
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, \
				      pattern, condition, source_offset, source_length) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition, \
	  source_offset, source_length },
static const struct orlix_tcti_target_completion_source_row source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct orlix_tcti_target_completion_semantic_provenance
semantic_provenance = {
#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(identity_value) identity_value
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(...)
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(...)
#include "../isa/target_asl_availability.def"
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(ordinal_value, leaf_name_value, \
		relative_file_value, decode_locator_value, decode_digest_value, \
		decode_sections_value, decode_helpers_value, decode_closure_value, \
		execute_locator_value, execute_digest_value, execute_sections_value, \
		execute_helpers_value, execute_closure_value) \
	{ .ordinal = ordinal_value, .name = leaf_name_value, \
	  .disposition = ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602, \
	  .relative_file = relative_file_value, \
	  .decode_locator = decode_locator_value, .decode_sha256 = decode_digest_value, \
	  .decode_section_count = decode_sections_value, \
	  .decode_helper_count = decode_helpers_value, \
	  .decode_helper_closure_sha256 = decode_closure_value, \
	  .execute_locator = execute_locator_value, \
	  .execute_sha256 = execute_digest_value, \
	  .execute_section_count = execute_sections_value, \
	  .execute_helper_count = execute_helpers_value, \
	  .execute_helper_closure_sha256 = execute_closure_value },
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(ordinal_value, \
		leaf_name_value, locator_value, source_offset_value, source_length_value, \
		digest_value) \
	{ .ordinal = ordinal_value, .name = leaf_name_value, \
	  .disposition = \
		ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_OFFICIAL_SEMANTICS_NOT_SPECIFIED, \
	  .aarchmrs_operation_locator = locator_value, \
	  .aarchmrs_operation_source_offset = source_offset_value, \
	  .aarchmrs_operation_source_length = source_length_value, \
	  .aarchmrs_operation_sha256 = digest_value },
static const struct orlix_tcti_target_completion_semantic_provenance_row
semantic_provenance_rows[] = {
#include "../isa/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

static const struct orlix_tcti_target_completion_system_accessor_provenance
system_accessor_provenance = {
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(architecture, build, release, schema, \
					timestamp, sha256) \
	architecture, build, release, schema, timestamp, sha256,
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(total, mapped, reserved, privileged, \
					unsupported, ambiguous, contradictory, \
					invalid) \
	total, mapped, reserved, privileged, unsupported, ambiguous, \
	contradictory, invalid,
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(identity) identity,
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(...)
#include "../isa/target_system_accessor_reconciliation.def"
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE
};

#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(accessor, encoding, name, generic, direction, \
				 disposition, selectors, condition, \
				 selector_identity, condition_identity, \
				 accessor_offset, accessor_length, \
				 encoding_offset, encoding_length, condition_offset, \
				 condition_length) \
	{ accessor, encoding, name, generic, direction, disposition, selectors, \
	  condition, selector_identity, condition_identity, accessor_offset, \
	  accessor_length, encoding_offset, encoding_length, condition_offset, \
	  condition_length },
static const struct orlix_tcti_target_completion_system_accessor_row
system_accessor_rows[] = {
#include "../isa/target_system_accessor_reconciliation.def"
};
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE

#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
				       canonical, evidence, proof) \
	{ stringify(name), classification, relation, canonical, evidence, proof },
static const struct orlix_tcti_target_completion_classification_row
classification_rows[] = {
#include "../isa/target_classification.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION

_Static_assert(sizeof(source_rows) / sizeof(source_rows[0]) ==
		       ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS,
	       "source manifest must contain exactly 4,350 rows");
_Static_assert(sizeof(classification_rows) / sizeof(classification_rows[0]) ==
		       ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS,
	       "classification ledger must contain exactly 4,350 rows");
_Static_assert(sizeof(semantic_provenance_rows) /
		       sizeof(semantic_provenance_rows[0]) ==
		       ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS,
	       "semantic provenance ledger must contain exactly 4,350 rows");
_Static_assert(sizeof(system_accessor_rows) / sizeof(system_accessor_rows[0]) ==
		       ORLIX_TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS,
	       "system accessor ledger must contain exactly 2,014 rows");

static bool empty(const char *text)
{
	return !text || !text[0];
}

static void record_error(struct orlix_tcti_target_completion_result *result,
			 orlix_tcti_completion_u32 error);
static int completion_audit_internal(
	const struct orlix_tcti_target_completion_audit_inputs_for_test *inputs,
	struct orlix_tcti_target_completion_result *result,
	struct orlix_tcti_target_completion_obligation *obligations);

static bool completion_projection_dependencies_valid(
	const struct orlix_tcti_target_completion_result *result)
{
	return result &&
		!(result->error_mask &
		  (ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_COUNT |
		   ORLIX_TCTI_TARGET_COMPLETION_ERROR_CLASSIFICATION_COUNT)) &&
		!result->invalid_source_rows &&
		!result->absent_rows && !result->stale_rows &&
		!result->invalid_relationship_rows &&
		!result->invalid_source_provenance &&
		!result->invalid_registry_entries &&
		!result->missing_semantic_provenance_rows &&
		!result->stale_semantic_provenance_rows &&
		!result->incompatible_semantic_provenance_rows &&
		!result->malformed_semantic_provenance_rows &&
		!result->dangling_semantic_provenance_rows &&
		!result->ambiguous_semantic_provenance_rows &&
		!result->invalid_system_accessor_rows &&
		!result->invalid_feature_field_domain_rows &&
		!result->invalid_runtime_capability_cohort_rows &&
		!result->missing_linux_proof_rows &&
		!result->duplicate_linux_proof_rows &&
		!result->stale_linux_proof_rows &&
		!result->malformed_linux_proof_rows &&
		!result->ambiguous_linux_proof_rows &&
		!result->invalid_linux_proof_provenance_rows &&
		!result->linux_proof_substitution_rows &&
		!result->invalid_feature_artifact &&
		!result->invalid_feature_applicability_artifact &&
		!result->invalid_source_condition_rows;
}

static int hex_nibble(char character, unsigned int *value)
{
	if (character >= '0' && character <= '9') {
		*value = (unsigned int)(character - '0');
		return 0;
	}
	if (character >= 'a' && character <= 'f') {
		*value = (unsigned int)(character - 'a' + 10);
		return 0;
	}
	if (character >= 'A' && character <= 'F') {
		*value = (unsigned int)(character - 'A' + 10);
		return 0;
	}
	return -1;
}

static bool source_condition_matches_artifact(
	const struct orlix_tcti_target_completion_source_row *source,
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf,
	const struct orlix_tcti_target_instruction_artifact *artifact)
{
	const char *hex;
	size_t index;

	if (!source || !leaf || !artifact || !artifact->condition_pool ||
	    !source->condition_tcnd_hex ||
	    leaf->condition_offset > artifact->condition_pool_size ||
	    leaf->condition_length > artifact->condition_pool_size -
		leaf->condition_offset)
		return false;
	hex = source->condition_tcnd_hex;
	if (strlen(hex) != (size_t)leaf->condition_length * 2U)
		return false;
	for (index = 0; index < leaf->condition_length; index++) {
		unsigned int high;
		unsigned int low;

		if (hex_nibble(hex[index * 2U], &high) ||
		    hex_nibble(hex[index * 2U + 1U], &low) ||
		artifact->condition_pool[leaf->condition_offset + index] !=
			(unsigned char)((high << 4) | low))
			return false;
	}
	return true;
}

static bool validate_completion_feature_dependencies(
	const struct orlix_tcti_feature_artifact *feature_artifact,
	const struct orlix_tcti_target_instruction_artifact *instruction_artifact,
	size_t source_count, struct orlix_tcti_target_completion_result *result)
{
	struct orlix_tcti_feature_artifact_scratch feature_scratch;
	struct orlix_tcti_feature_artifact_diagnostic feature_diagnostic;
	struct orlix_tcti_target_instruction_artifact_validation_result
		instruction_diagnostic;
	static orlix_tcti_feature_artifact_u8 feature_state[
		ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static struct orlix_tcti_feature_artifact_frame feature_frames[
		ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static orlix_tcti_feature_artifact_u8 feature_child_coverage[
		ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COUNT];

	feature_scratch = (struct orlix_tcti_feature_artifact_scratch) {
		.state = feature_state,
		.state_count = sizeof(feature_state),
		.frames = feature_frames,
		.frame_count = sizeof(feature_frames) / sizeof(feature_frames[0]),
		.child_coverage = feature_child_coverage,
		.child_coverage_count = sizeof(feature_child_coverage),
	};
	if (orlix_tcti_feature_artifact_validate(feature_artifact, &feature_scratch,
					  &feature_diagnostic) !=
		ORLIX_TCTI_FEATURE_ARTIFACT_VALID ||
	    orlix_tcti_target_instruction_artifact_validate(instruction_artifact,
					      &instruction_diagnostic) ||
	    instruction_artifact->leaf_count != source_count) {
		result->invalid_feature_artifact++;
		record_error(result, ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_DOMAIN);
		return false;
	}
	return true;
}

static void validate_source_feature_domain(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_feature_artifact *feature_artifact,
	const struct orlix_tcti_target_instruction_artifact *instruction_artifact,
	const struct orlix_tcti_target_feature_applicability_artifact *applicability_artifact,
	struct orlix_tcti_target_completion_result *result,
	struct orlix_tcti_target_completion_obligation *obligations)
{
	struct orlix_tcti_target_feature_applicability_validation_result diagnostic;
	static unsigned char semantic_active[ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static unsigned char semantic_common_seen[
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_MAX_COMMON_VALUES];
	static size_t semantic_parameter_order[
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PARAMETERS];
	static size_t semantic_binding_order[
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_OCCURRENCE_COUNT];
	static size_t semantic_field_common_index[
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT];
	struct orlix_tcti_target_feature_applicability_semantic_scratch
		semantic_scratch = {
			.active_feature_nodes = semantic_active,
			.active_feature_node_count = sizeof(semantic_active),
			.common_values_seen = semantic_common_seen,
			.common_values_seen_count = sizeof(semantic_common_seen),
			.parameter_order = semantic_parameter_order,
			.parameter_order_count = sizeof(semantic_parameter_order) /
				sizeof(semantic_parameter_order[0]),
			.binding_order = semantic_binding_order,
			.binding_order_count = sizeof(semantic_binding_order) /
				sizeof(semantic_binding_order[0]),
			.field_common_index = semantic_field_common_index,
			.field_common_index_count = sizeof(semantic_field_common_index) /
				sizeof(semantic_field_common_index[0]),
		};
	size_t index;

	if (!applicability_artifact ||
	    orlix_tcti_target_feature_applicability_artifact_validate_semantics(
		applicability_artifact, &semantic_scratch, &diagnostic) ||
	    strcmp(applicability_artifact->provenance->instructions_sha256,
		   source_provenance.source_sha256) ||
	    strcmp(applicability_artifact->provenance->features_sha256,
		   feature_artifact->source.sha256) ||
	    applicability_artifact->row_count != source_count) {
		result->invalid_feature_applicability_artifact++;
		result->invalid_feature_applicability_rows += source_count;
		result->unresolved_feature_applicability_rows += source_count;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_APPLICABILITY);
		if (obligations) {
			for (index = 0; index < source_count; index++) {
				obligations[index].feature_union_state =
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_INVALID;
				obligations[index].first_unsupported_error =
					ORLIX_TCTI_FEATURE_DOMAIN_TCND_INVALID_ARGUMENT;
				obligations[index].known_feature_union_reason_mask =
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INVALID |
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INCOMPLETE;
				obligations[index].blocker_mask |=
					ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION |
					ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION_INCOMPLETE;
			}
		}
		return;
	}

	for (index = 0; index < source_count; index++) {
		struct orlix_tcti_feature_domain_tcnd_diagnostic condition_diagnostic;
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
			&instruction_artifact->leaves[index];
		const struct orlix_tcti_target_feature_applicability_row *row =
			&applicability_artifact->rows[index];

		if (!source_condition_matches_artifact(&source[index], leaf,
					       instruction_artifact) ||
		    orlix_tcti_feature_domain_validate_tcnd_features(
			feature_artifact, source[index].condition_tcnd_hex,
			&condition_diagnostic)) {
			result->invalid_source_condition_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_DOMAIN);
			if (obligations) {
				obligations[index].blocker_mask |=
					ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_SOURCE_CONDITION;
				obligations[index].feature_union_state =
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_INVALID;
				obligations[index].first_unsupported_error =
					ORLIX_TCTI_FEATURE_DOMAIN_TCND_INVALID_ARGUMENT;
				obligations[index].known_feature_union_reason_mask |=
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INVALID;
			}
			continue;
		}
		result->source_condition_domain_bound_rows++;
		if (row->ordinal != index ||
		    strcmp(row->name, source[index].name) ||
		    strcmp(row->mnemonic, source[index].mnemonic) ||
		    strcmp(row->operation_id, source[index].operation_id)) {
			result->invalid_feature_applicability_rows++;
			result->unresolved_feature_applicability_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_APPLICABILITY);
			if (obligations) {
				obligations[index].feature_union_state =
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_INVALID;
				obligations[index].first_unsupported_error =
					ORLIX_TCTI_FEATURE_DOMAIN_TCND_INVALID_ARGUMENT;
				obligations[index].known_feature_union_reason_mask |=
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INVALID |
					ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INCOMPLETE;
				obligations[index].blocker_mask |=
					ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION |
					ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION_INCOMPLETE;
			}
			continue;
		}
		result->evaluated_feature_applicability_rows++;
		if (row->status == ORLIX_TCTI_TARGET_FEATURE_APPLICABLE)
			result->satisfied_feature_applicability_rows++;
		else
			result->unsatisfied_feature_applicability_rows++;
		if (obligations) {
			obligations[index].first_unsupported_error =
				ORLIX_TCTI_FEATURE_DOMAIN_TCND_OK;
			obligations[index].feature_union_state =
				ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_EVALUATED;
			obligations[index].known_feature_union_reason_mask =
				ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_NONE;
			obligations[index].blocker_mask &=
				~(ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION |
				  ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION_INCOMPLETE);
		}
	}
}

int orlix_tcti_target_completion_validate_feature_field_domains(
	const struct orlix_tcti_feature_artifact *feature_artifact,
	const struct orlix_tcti_feature_field_domain_binding_artifact *artifact,
	struct orlix_tcti_target_completion_result *result)
{
	struct orlix_tcti_feature_field_domain_binding_scratch scratch;
	struct orlix_tcti_feature_field_domain_binding_diagnostic diagnostic;
	static orlix_tcti_feature_artifact_u8 feature_node_coverage[
		ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static orlix_tcti_feature_artifact_u32 identity_group_coverage[
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT];
	orlix_tcti_feature_artifact_u32 index;

	if (!result)
		return -1;
	scratch = (struct orlix_tcti_feature_field_domain_binding_scratch) {
		.feature_node_coverage = feature_node_coverage,
		.feature_node_coverage_count = sizeof(feature_node_coverage),
		.identity_group_coverage = identity_group_coverage,
		.identity_group_coverage_count =
			sizeof(identity_group_coverage) /
			sizeof(identity_group_coverage[0]),
	};
	if (orlix_tcti_feature_field_domain_binding_validate(feature_artifact, artifact,
						       &scratch, &diagnostic) !=
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID) {
		result->invalid_feature_field_domain_rows++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
		return -1;
	}
	for (index = 0; index < artifact->occurrence_count; index++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&artifact->bindings[index];

		result->feature_field_domain_rows++;
		if (binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED) {
			result->mapped_feature_field_domain_rows++;
		} else {
			result->ambiguous_feature_field_domain_rows++;
		}
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
	}
	return -1;
}

int orlix_tcti_target_completion_validate_runtime_capability_cohorts(
	const struct orlix_tcti_runtime_capability_cohort_artifact *artifact,
	struct orlix_tcti_target_completion_result *result)
{
	struct orlix_tcti_runtime_capability_cohort_validation_result diagnostic;
	size_t leaf_index;

	if (!result)
		return -1;
	if (orlix_tcti_runtime_capability_cohort_artifact_validate(artifact,
						     &diagnostic)) {
		result->invalid_runtime_capability_cohort_rows++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT);
		return -1;
	}
	for (leaf_index = 0; leaf_index < artifact->counts.leaf_count;
	     leaf_index++) {
		const struct orlix_tcti_runtime_capability_cohort_leaf *leaf =
			&artifact->leaves[leaf_index];
		size_t membership_index;

		result->runtime_capability_cohort_leaf_rows++;
		for (membership_index = leaf->first_membership;
		     membership_index < leaf->first_membership + leaf->membership_count;
		     membership_index++) {
			const struct orlix_tcti_runtime_capability_cohort_membership *member =
				&artifact->memberships[membership_index];

			result->runtime_capability_cohort_candidate_membership_rows++;
			if (member->disposition ==
			    ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED) {
				result->unresolved_runtime_capability_cohort_membership_rows++;
				record_error(result,
					     ORLIX_TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT);
			}
		}
	}
	return result->unresolved_runtime_capability_cohort_membership_rows ?
		-1 : 0;
}

static bool source_row_well_formed(
	const struct orlix_tcti_target_completion_source_row *row, size_t ordinal)
{
	return row && row->ordinal == ordinal && !empty(row->name) &&
		!empty(row->mnemonic) && !empty(row->operation_id) &&
		!empty(row->condition_tcnd_hex) && row->source_length &&
		row->source_offset < ORLIX_TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH &&
		row->source_length <=
			ORLIX_TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH - row->source_offset;
}

static bool source_row_matches_canonical(
	const struct orlix_tcti_target_completion_source_row *row, size_t ordinal)
{
	const struct orlix_tcti_target_completion_source_row *canonical;

	if (!source_row_well_formed(row, ordinal) ||
	    ordinal >= sizeof(source_rows) / sizeof(source_rows[0]))
		return false;
	canonical = &source_rows[ordinal];
	return !strcmp(row->name, canonical->name) &&
		!strcmp(row->mnemonic, canonical->mnemonic) &&
		!strcmp(row->operation_id, canonical->operation_id) &&
		row->mask == canonical->mask &&
		row->pattern == canonical->pattern &&
		!strcmp(row->condition_tcnd_hex,
			canonical->condition_tcnd_hex) &&
		row->source_offset == canonical->source_offset &&
		row->source_length == canonical->source_length;
}

static bool sha256_hex_is_valid(const char *value)
{
	size_t index;

	if (!value || strlen(value) != 64U)
		return false;
	for (index = 0; index < 64U; index++)
		if (!((value[index] >= '0' && value[index] <= '9') ||
		      (value[index] >= 'a' && value[index] <= 'f')))
			return false;
	return true;
}

static bool semantic_provenance_identity_is_valid(
	const struct orlix_tcti_target_completion_semantic_provenance *provenance)
{
	static const char archive[] =
		"archive_sha256=63a01a1696483bbe2edfef9e0f0cd053d6c1c619ec0587876cb7a60bb344f354";
	static const char release[] =
		"release_digest=bbe8309a4c746a996c84a3db7c92fa2a6a7453e6915231570f23e2d361bfdccd";
	static const char aarchmrs[] =
		"aarchmrs=vFATAp1-A/build=818/ref=2026-06_rel";
	static const char instructions[] =
		"instructions_sha256=a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe";
	static const char shared[] =
		"shared_pseudocode_sha256=21edeadbc26408a35bcf7315bfbcbb8a69e9e6d86d952c80b153d273f0f2349f";
	static const char notice[] =
		"notice_sha256=9c2cc480e9706819e0d295329cf7f94f9256c610dfa9bc8f055a05389f74704a";
	static const char index[] =
		"index_sha256=905dd3dd5537ff89514e21ce0d43bf180b84d2c45e5e498dfdcc502165185b93";

	return provenance && !empty(provenance->identity) &&
		!empty(semantic_provenance.identity) &&
		!strcmp(provenance->identity, semantic_provenance.identity) &&
		strstr(provenance->identity,
		       "authority=Arm_DDI0602_2026_06") &&
		strstr(provenance->identity,
		       "distribution=external_non_redistributed") &&
		strstr(provenance->identity, archive) &&
		strstr(provenance->identity, release) &&
		strstr(provenance->identity, aarchmrs) &&
		strstr(provenance->identity, instructions) &&
		strstr(provenance->identity, shared) &&
		strstr(provenance->identity, notice) &&
		strstr(provenance->identity, index) &&
		strstr(provenance->identity, "ddi0602_rows=4332") &&
		strstr(provenance->identity,
		       "official_semantics_not_specified_rows=18");
}

static bool semantic_provenance_identity_is_compatible(
	const struct orlix_tcti_target_completion_semantic_provenance *provenance)
{
	return provenance && !empty(provenance->identity) &&
		strstr(provenance->identity, "authority=Arm_DDI0602_2026_06") &&
		strstr(provenance->identity,
		       "distribution=external_non_redistributed") &&
		strstr(provenance->identity,
		       "aarchmrs=vFATAp1-A/build=818/ref=2026-06_rel");
}

static bool nullable_string_equal(const char *left, const char *right)
{
	if (!left || !right)
		return left == right;
	return !strcmp(left, right);
}

static bool segment_has_suffix(const char *segment, size_t segment_length,
			       const char *suffix)
{
	size_t suffix_length = strlen(suffix);

	return segment_length >= suffix_length &&
		!strncmp(segment + segment_length - suffix_length, suffix,
			 suffix_length);
}

static bool external_locator_list_is_valid(const char *relative_file,
					   const char *locators,
					   orlix_tcti_completion_u32 section_count,
					   bool decode)
{
	const char *cursor;
	size_t file_length;
	orlix_tcti_completion_u32 observed = 0;

	if (empty(relative_file) || empty(locators) || !section_count ||
	    relative_file[0] == '/' || strstr(relative_file, ".."))
		return false;
	file_length = strlen(relative_file);
	if (file_length <= 4U ||
	    strcmp(relative_file + file_length - 4U, ".xml"))
		return false;
	for (cursor = locators; *cursor;) {
		const char *end = strchr(cursor, ';');
		size_t length = end ? (size_t)(end - cursor) : strlen(cursor);

		if (length <= file_length ||
		    strncmp(cursor, relative_file, file_length) ||
		    cursor[file_length] != '#')
			return false;
		if (decode) {
			if (!segment_has_suffix(cursor, length, "/decode") &&
			    !segment_has_suffix(cursor, length, "/postdecode"))
				return false;
		} else if (!segment_has_suffix(cursor, length, "/execute")) {
			return false;
		}
		observed++;
		if (!end)
			break;
		cursor = end + 1;
		if (!*cursor)
			return false;
	}
	return observed == section_count;
}

static bool semantic_provenance_row_is_well_formed(
	const struct orlix_tcti_target_completion_semantic_provenance_row *row)
{
	if (!row || empty(row->name))
		return false;
	if (row->disposition ==
	    ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602)
		return external_locator_list_is_valid(
				row->relative_file, row->decode_locator,
				row->decode_section_count, true) &&
			external_locator_list_is_valid(
					row->relative_file, row->execute_locator,
					row->execute_section_count, false) &&
			strcmp(row->decode_locator, row->execute_locator) &&
			sha256_hex_is_valid(row->decode_sha256) &&
			sha256_hex_is_valid(row->decode_helper_closure_sha256) &&
			sha256_hex_is_valid(row->execute_sha256) &&
			sha256_hex_is_valid(row->execute_helper_closure_sha256) &&
			empty(row->aarchmrs_operation_locator) &&
			!row->aarchmrs_operation_source_offset &&
			!row->aarchmrs_operation_source_length &&
			empty(row->aarchmrs_operation_sha256);
	if (row->disposition ==
	    ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_OFFICIAL_SEMANTICS_NOT_SPECIFIED)
		return empty(row->relative_file) && empty(row->decode_locator) &&
			empty(row->decode_sha256) && !row->decode_section_count &&
			!row->decode_helper_count &&
			empty(row->decode_helper_closure_sha256) &&
			empty(row->execute_locator) && empty(row->execute_sha256) &&
			!row->execute_section_count && !row->execute_helper_count &&
			empty(row->execute_helper_closure_sha256) &&
			!empty(row->aarchmrs_operation_locator) &&
			row->aarchmrs_operation_source_length &&
			row->aarchmrs_operation_source_offset <
				ORLIX_TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH &&
			row->aarchmrs_operation_source_length <=
				ORLIX_TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH -
				row->aarchmrs_operation_source_offset &&
			sha256_hex_is_valid(row->aarchmrs_operation_sha256);
	return false;
}

static bool semantic_provenance_row_matches_canonical(
	const struct orlix_tcti_target_completion_semantic_provenance_row *row,
	const struct orlix_tcti_target_completion_semantic_provenance_row *canonical)
{
	return row->ordinal == canonical->ordinal &&
		!strcmp(row->name, canonical->name) &&
		row->disposition == canonical->disposition &&
		nullable_string_equal(row->relative_file, canonical->relative_file) &&
		nullable_string_equal(row->decode_locator, canonical->decode_locator) &&
		nullable_string_equal(row->decode_sha256, canonical->decode_sha256) &&
		row->decode_section_count == canonical->decode_section_count &&
		row->decode_helper_count == canonical->decode_helper_count &&
		nullable_string_equal(row->decode_helper_closure_sha256,
				      canonical->decode_helper_closure_sha256) &&
		nullable_string_equal(row->execute_locator, canonical->execute_locator) &&
		nullable_string_equal(row->execute_sha256, canonical->execute_sha256) &&
		row->execute_section_count == canonical->execute_section_count &&
		row->execute_helper_count == canonical->execute_helper_count &&
		nullable_string_equal(row->execute_helper_closure_sha256,
				      canonical->execute_helper_closure_sha256) &&
		nullable_string_equal(row->aarchmrs_operation_locator,
				      canonical->aarchmrs_operation_locator) &&
		row->aarchmrs_operation_source_offset ==
			canonical->aarchmrs_operation_source_offset &&
		row->aarchmrs_operation_source_length ==
			canonical->aarchmrs_operation_source_length &&
		nullable_string_equal(row->aarchmrs_operation_sha256,
				      canonical->aarchmrs_operation_sha256);
}

int orlix_tcti_target_completion_validate_semantic_provenance(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_semantic_provenance *provenance,
	const struct orlix_tcti_target_completion_semantic_provenance_row *rows,
	size_t row_count,
	struct orlix_tcti_target_completion_result *result)
{
	bool seen[ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS] = { false };
	size_t index;

	if (!result)
		return -1;
	if (!source || source_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS ||
	    !rows) {
		result->missing_semantic_provenance_rows +=
			ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
		return -1;
	}
	if (!semantic_provenance_identity_is_valid(provenance)) {
		if (!provenance || empty(provenance->identity))
			result->malformed_semantic_provenance_rows++;
		else if (!semantic_provenance_identity_is_compatible(provenance))
			result->incompatible_semantic_provenance_rows++;
		else
			result->stale_semantic_provenance_rows++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
	}

	for (index = 0; index < row_count; index++) {
		const struct orlix_tcti_target_completion_semantic_provenance_row *row =
			&rows[index];

		if (row->ordinal >= ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS) {
			result->dangling_semantic_provenance_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
			continue;
		}
		if (seen[row->ordinal]) {
			result->ambiguous_semantic_provenance_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
			continue;
		}
		seen[row->ordinal] = true;
		if (!source_row_well_formed(&source[row->ordinal], row->ordinal) ||
		    strcmp(row->name, source[row->ordinal].name)) {
			result->dangling_semantic_provenance_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
			continue;
		}
		if (!semantic_provenance_row_is_well_formed(row)) {
			result->malformed_semantic_provenance_rows++;
			if (!result->first_invalid_semantic_provenance_ordinal_plus_one)
				result->first_invalid_semantic_provenance_ordinal_plus_one =
					row->ordinal + 1U;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
			continue;
		}
		if (!semantic_provenance_row_matches_canonical(
			    row, &semantic_provenance_rows[row->ordinal])) {
			result->stale_semantic_provenance_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
			continue;
		}
		result->semantic_provenance_rows++;
		if (row->disposition ==
		    ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602) {
			result->external_ddi0602_semantic_provenance_rows++;
		} else {
			result->official_semantics_not_specified_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_OFFICIAL_SEMANTICS_NOT_SPECIFIED);
		}
	}
	for (index = 0; index < ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS; index++)
		if (!seen[index])
			result->missing_semantic_provenance_rows++;
	if (row_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS ||
	    result->missing_semantic_provenance_rows) {
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);
	}
	return result->missing_semantic_provenance_rows ||
		result->stale_semantic_provenance_rows ||
		result->incompatible_semantic_provenance_rows ||
		result->malformed_semantic_provenance_rows ||
		result->dangling_semantic_provenance_rows ||
		result->ambiguous_semantic_provenance_rows ||
		result->official_semantics_not_specified_rows ? -1 : 0;
}

static void record_error(struct orlix_tcti_target_completion_result *result,
			 orlix_tcti_completion_u32 error)
{
	result->error_mask |= error;
	result->errors++;
}

static size_t find_source(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count, const char *name)
{
	size_t index;

	if (empty(name))
		return source_count;
	for (index = 0; index < source_count; index++)
		if (source_row_well_formed(&source[index], index) &&
		    !strcmp(source[index].name, name))
			return index;
	return source_count;
}

static bool register_source_span_valid(orlix_tcti_completion_u64 offset, orlix_tcti_completion_u64 length)
{
	return length && offset < ORLIX_TCTI_TARGET_COMPLETION_REGISTERS_BYTE_LENGTH &&
	       length <= ORLIX_TCTI_TARGET_COMPLETION_REGISTERS_BYTE_LENGTH - offset;
}

static orlix_tcti_completion_u64 accessor_identity_byte(orlix_tcti_completion_u64 identity, unsigned char byte)
{
	return (identity ^ byte) * ORLIX_TCTI_COMPLETION_U64_C(1099511628211);
}

static orlix_tcti_completion_u64 accessor_identity_u64(orlix_tcti_completion_u64 identity, orlix_tcti_completion_u64 value)
{
	unsigned int index;

	for (index = 0; index < 8U; index++)
		identity = accessor_identity_byte(identity,
			(unsigned char)(value >> (index * 8U)));
	return identity;
}

static orlix_tcti_completion_u64 accessor_identity_text(orlix_tcti_completion_u64 identity, const char *text)
{
	size_t length = strlen(text);
	size_t index;

	identity = accessor_identity_u64(identity, length);
	for (index = 0; index < length; index++)
		identity = accessor_identity_byte(identity, (unsigned char)text[index]);
	return identity;
}

static orlix_tcti_completion_u64 system_accessor_identity(
	const struct orlix_tcti_target_completion_system_accessor_row *accessors,
	size_t accessor_count)
{
	orlix_tcti_completion_u64 identity = ORLIX_TCTI_COMPLETION_U64_C(1469598103934665603);
	size_t index;

	identity = accessor_identity_u64(identity, accessor_count);
	for (index = 0; index < accessor_count; index++) {
		const struct orlix_tcti_target_completion_system_accessor_row *row =
			&accessors[index];

		identity = accessor_identity_u64(identity, row->accessor_index);
		identity = accessor_identity_u64(identity, row->encoding_index);
		identity = accessor_identity_text(identity, row->name);
		identity = accessor_identity_text(identity, row->generic_leaf);
		identity = accessor_identity_u64(identity, row->direction);
		identity = accessor_identity_u64(identity, row->disposition);
		identity = accessor_identity_u64(identity, row->selector_count);
		identity = accessor_identity_u64(identity, row->condition_expression);
		identity = accessor_identity_u64(identity, row->selector_identity);
		identity = accessor_identity_u64(identity, row->condition_identity);
		identity = accessor_identity_u64(identity,
			row->accessor_source_offset);
		identity = accessor_identity_u64(identity,
			row->accessor_source_length);
		identity = accessor_identity_u64(identity,
			row->encoding_source_offset);
		identity = accessor_identity_u64(identity,
			row->encoding_source_length);
		identity = accessor_identity_u64(identity,
			row->condition_source_offset);
		identity = accessor_identity_u64(identity,
			row->condition_source_length);
	}
	return identity;
}

int orlix_tcti_target_completion_validate_system_accessors(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_system_accessor_provenance *provenance,
	const struct orlix_tcti_target_completion_system_accessor_row *accessors,
	size_t accessor_count,
	struct orlix_tcti_target_completion_result *result)
{
	size_t actual[7] = { 0 };
	size_t index;

	if (!result)
		return -1;
	result->system_accessor_rows = accessor_count;
	if (!source || source_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS ||
	    !provenance || !accessors ||
	    empty(provenance->architecture) || empty(provenance->build) ||
	    empty(provenance->release) || empty(provenance->schema) ||
	    empty(provenance->timestamp) || empty(provenance->source_sha256) ||
	    strcmp(provenance->architecture, "vFATAp1-A") ||
	    strcmp(provenance->build, "818") ||
	    strcmp(provenance->release, "2026-06_rel") ||
	    strcmp(provenance->schema, "2.9.5") ||
	    strcmp(provenance->timestamp, "2026-06-24 17:12:14") ||
	    strcmp(provenance->source_sha256,
		   "5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874") ||
	    provenance->accessor_count !=
		    ORLIX_TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS ||
	    accessor_count != ORLIX_TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS ||
	    provenance->mapped_count + provenance->reserved_count +
			    provenance->privileged_count +
			    provenance->unsupported_count +
			    provenance->ambiguous_count +
			    provenance->contradictory_count +
			    provenance->invalid_count !=
		    provenance->accessor_count) {
		result->invalid_system_accessor_rows++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
		if (!source || !provenance || !accessors)
			return -1;
	}

	for (index = 0; index < accessor_count; index++) {
		const struct orlix_tcti_target_completion_system_accessor_row *row =
			&accessors[index];
		size_t previous;
		bool valid = !empty(row->name) &&
			!strncmp(row->name, "A64.", 4U) &&
			row->direction >=
				ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_READ &&
			row->direction <=
				ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_EXECUTE &&
			row->disposition >=
				ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED &&
			row->disposition <=
				ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_INVALID &&
			register_source_span_valid(row->accessor_source_offset,
						   row->accessor_source_length);

		if (valid && row->disposition ==
				     ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED)
			valid = row->encoding_index != UINT32_MAX &&
				!empty(row->generic_leaf) &&
				row->selector_count &&
				row->selector_identity && row->condition_identity &&
				row->condition_expression != UINT32_MAX &&
				register_source_span_valid(
					row->condition_source_offset,
					row->condition_source_length) &&
				register_source_span_valid(
					row->encoding_source_offset,
					row->encoding_source_length) &&
				find_source(source, source_count,
					    row->generic_leaf) < source_count;
		for (previous = 0; valid && previous < index; previous++)
			if (accessors[previous].accessor_index ==
				    row->accessor_index ||
			    (row->encoding_index != UINT32_MAX &&
			     accessors[previous].encoding_index ==
				     row->encoding_index))
				valid = false;
		if (!valid) {
			result->invalid_system_accessor_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
			continue;
		}
		actual[row->disposition]++;
		if (row->disposition ==
		    ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED) {
			result->mapped_system_accessor_rows++;
		} else {
			result->nonmapped_system_accessor_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
		}
	}
	if (actual[ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED] !=
		    provenance->mapped_count ||
	    actual[ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_RESERVED] !=
		    provenance->reserved_count ||
	    actual[ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_PRIVILEGED] !=
		    provenance->privileged_count ||
	    actual[ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_UNSUPPORTED] !=
		    provenance->unsupported_count ||
	    actual[ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_AMBIGUOUS] !=
		    provenance->ambiguous_count ||
	    actual[ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_CONTRADICTORY] !=
		    provenance->contradictory_count ||
	    actual[ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_INVALID] !=
	    provenance->invalid_count) {
		result->invalid_system_accessor_rows++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
	}
	if (system_accessor_identity(accessors, accessor_count) !=
		    provenance->reconciliation_identity) {
		result->invalid_system_accessor_rows++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
	}
	return result->invalid_system_accessor_rows ||
		       result->nonmapped_system_accessor_rows ?
	       -1 : 0;
}

static size_t find_classification(
	const struct orlix_tcti_target_completion_classification_row *classification,
	size_t classification_count, const char *name)
{
	size_t index;

	if (empty(name))
		return classification_count;
	for (index = 0; index < classification_count; index++)
		if (!empty(classification[index].name) &&
		    !strcmp(classification[index].name, name))
			return index;
	return classification_count;
}

static bool exact_binding(
	const struct orlix_tcti_target_completion_source_row *source,
	const struct orlix_tcti_target_proof_binding *binding)
{
	return source && !empty(source->name) && !empty(source->mnemonic) &&
	       !empty(source->condition_tcnd_hex) &&
	       !strcmp(source->name, binding->leaf_name) &&
	       !strcmp(source->mnemonic, binding->mnemonic) &&
	       source->mask == binding->encoding_mask &&
	       source->pattern == binding->encoding_pattern &&
	       !strcmp(source->condition_tcnd_hex,
		       binding->condition_tcnd_hex);
}

static bool registry_binds_source(
	const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count, const char *proof_id,
	const struct orlix_tcti_target_completion_source_row *source)
{
	size_t entry_index;

	for (entry_index = 0; entry_index < registry_count; entry_index++) {
		size_t binding_index;

		if (empty(registry[entry_index].id) ||
		    empty(registry[entry_index].operation_id) ||
		    strcmp(registry[entry_index].id, proof_id))
			continue;
		for (binding_index = 0;
		     binding_index < registry[entry_index].binding_count;
		     binding_index++)
			if (!strcmp(registry[entry_index].operation_id,
				    source->operation_id) &&
			    exact_binding(
				    source,
				    &registry[entry_index]
					     .bindings[binding_index]))
				return true;
		return false;
	}
	return false;
}

static bool valid_non_alias_relationship(
	const struct orlix_tcti_target_completion_classification_row *row)
{
	return row->relation == ORLIX_TCTI_TARGET_COMPLETION_RELATION_NONE &&
	       empty(row->canonical_name);
}

static bool valid_alias_relationship(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_classification_row *classification,
	size_t classification_count, size_t row_index)
{
	const struct orlix_tcti_target_completion_classification_row *row =
		&classification[row_index];
	size_t canonical_classification;
	size_t canonical_source;

	if (row->relation != ORLIX_TCTI_TARGET_COMPLETION_RELATION_ALIAS &&
	    row->relation != ORLIX_TCTI_TARGET_COMPLETION_RELATION_DUPLICATE)
		return false;
	if (empty(row->canonical_name) || empty(row->evidence) ||
	    empty(row->proof_id) ||
	    !strcmp(row->name, row->canonical_name))
		return false;
	canonical_classification = find_classification(
		classification, classification_count, row->canonical_name);
	canonical_source = find_source(source, source_count,
				       row->canonical_name);
	if (canonical_classification == classification_count ||
	    canonical_source == source_count ||
	    classification[canonical_classification].classification ==
		    ORLIX_TCTI_TARGET_COMPLETION_UNCLASSIFIED ||
	    classification[canonical_classification].classification ==
		    ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE ||
	    classification[canonical_classification].relation !=
		    ORLIX_TCTI_TARGET_COMPLETION_RELATION_NONE ||
	    !empty(classification[canonical_classification].canonical_name) ||
	    empty(classification[canonical_classification].evidence) ||
	    empty(classification[canonical_classification].proof_id))
		return false;
	/*
	 * AARCHMRS InstructionAlias and OperationAlias records may resolve a
	 * distinct operation or constrained encoding. The pinned source records
	 * the relationship as a provenance edge, not source-row identity.
	 */
	return true;
}

static bool classification_row_well_formed(
	const struct orlix_tcti_target_completion_classification_row *row)
{
	return row && !empty(row->name);
}

/*
 * This validates immutable source-to-test provenance. It does not observe a
 * KUnit or kselftest result and therefore must not be described as proof.
 */
static bool source_proof_binding_resolves(
	const struct orlix_tcti_target_completion_source_row *source,
	const struct orlix_tcti_target_completion_classification_row *row,
	const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count)
{
	const struct orlix_tcti_target_proof_reference reference = {
		.id = row->proof_id,
		.leaf_name = source->name,
		.mnemonic = source->mnemonic,
		.operation_id = source->operation_id,
		.encoding_mask = source->mask,
		.encoding_pattern = source->pattern,
		.condition_tcnd_hex = source->condition_tcnd_hex,
		.classification = row->classification,
	};

	return orlix_tcti_target_proof_registry_lookup(
		       registry, registry_count, &reference) ==
	       ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK;
}

static void validate_registry_bindings(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_classification_row *classification,
	size_t classification_count,
	const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count, struct orlix_tcti_target_completion_result *result)
{
	size_t entry_index;

	for (entry_index = 0; entry_index < registry_count; entry_index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&registry[entry_index];
		size_t binding_index;

		for (binding_index = 0;
		     binding_index < entry->binding_count;
		     binding_index++) {
			const struct orlix_tcti_target_proof_binding *binding =
				&entry->bindings[binding_index];
			size_t source_index =
				find_source(source, source_count,
					    binding->leaf_name);
			size_t classification_index =
				find_classification(classification,
						    classification_count,
						    binding->leaf_name);
			bool valid = source_index < source_count &&
				classification_index < classification_count;

			/*
			 * An unclassified row may carry an incomplete ownership
			 * binding without claiming classification proof.
			 */
			if (valid && entry->unproved_obligations &&
			    classification[classification_index].classification ==
				    ORLIX_TCTI_TARGET_COMPLETION_UNCLASSIFIED)
				continue;
			if (valid)
				valid = exact_binding(&source[source_index],
						      binding) &&
					!empty(source[source_index]
						       .operation_id) &&
					!strcmp(source[source_index].operation_id,
						entry->operation_id) &&
					classification[classification_index]
							.classification < 32 &&
					entry->classification_mask ==
						(1U << classification[
							classification_index]
								.classification) &&
					!empty(classification[
							classification_index]
								.proof_id) &&
					!strcmp(classification[
							classification_index]
								.proof_id,
						entry->id);
			if (!valid) {
				result->stale_proof_bindings++;
				record_error(
					result,
					ORLIX_TCTI_TARGET_COMPLETION_ERROR_STALE_PROOF_BINDING);
			}
		}
	}
}

static void validate_unproved_obligations(
	const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count, struct orlix_tcti_target_completion_result *result)
{
	size_t entry_index;

	for (entry_index = 0; entry_index < registry_count; entry_index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&registry[entry_index];
		size_t binding_index;

		if (!entry->unproved_obligations)
			continue;
		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++) {
			result->unproved_obligation_bindings++;
			record_error(
				result,
				ORLIX_TCTI_TARGET_COMPLETION_ERROR_UNPROVED_OBLIGATIONS);
		}
	}
}

int orlix_tcti_target_completion_validate(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_classification_row *classification,
	size_t classification_count,
	const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count,
	struct orlix_tcti_target_completion_result *result)
{
	enum orlix_tcti_target_proof_registry_error registry_error;
	bool registry_valid = true;
	size_t index;

	if (!result)
		return -1;
	memset(result, 0, sizeof(*result));
	result->source_rows = source_count;
	result->classification_rows = classification_count;

	if (!source || source_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS) {
		record_error(result, ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_COUNT);
		if (!source)
			return -1;
	}
	if (!classification ||
	    classification_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS) {
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_CLASSIFICATION_COUNT);
		if (!classification)
			return -1;
	}
	if (orlix_tcti_target_proof_registry_validate(registry, registry_count,
						&registry_error)) {
		registry_valid = false;
		result->invalid_registry_entries++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_PROOF_REGISTRY);
	}

	for (index = 0; index < source_count; index++) {
		size_t previous;

		if (!source_row_matches_canonical(&source[index], index)) {
			result->invalid_source_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE);
		}
		for (previous = 0; previous < index; previous++)
			if (source_row_well_formed(&source[index], index) &&
			    source_row_well_formed(&source[previous], previous) &&
			    !strcmp(source[index].name,
				    source[previous].name)) {
				result->invalid_source_rows++;
				record_error(
					result,
					ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE);
				break;
			}
		if (source_row_well_formed(&source[index], index) &&
		    find_classification(classification,
					classification_count,
					source[index].name) ==
		    classification_count) {
			result->absent_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_ABSENT);
		}
	}

	/* Never consume an invalid registry in binding or proof lookup paths. */
	if (registry_valid)
		validate_registry_bindings(source, source_count, classification,
					   classification_count, registry,
					   registry_count, result);
	if (registry_valid)
		validate_unproved_obligations(registry, registry_count, result);

	for (index = 0; index < classification_count; index++) {
		const struct orlix_tcti_target_completion_classification_row *row =
			&classification[index];
		size_t source_index =
			find_source(source, source_count, row->name);
		size_t previous;
		bool relationship_valid;

		if (!classification_row_well_formed(row) ||
		    source_index == source_count) {
			result->stale_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_STALE);
			continue;
		}
		for (previous = 0; previous < index; previous++)
			if (classification_row_well_formed(row) &&
			    classification_row_well_formed(&classification[previous]) &&
			    !strcmp(row->name,
				    classification[previous].name)) {
				result->stale_rows++;
				record_error(result,
					     ORLIX_TCTI_TARGET_COMPLETION_ERROR_STALE);
				break;
			}
		if (row->classification ==
		    ORLIX_TCTI_TARGET_COMPLETION_UNCLASSIFIED) {
			result->unclassified_rows++;
			record_error(
				result,
				ORLIX_TCTI_TARGET_COMPLETION_ERROR_UNCLASSIFIED);
			continue;
		}
		if (row->classification >
		    ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE) {
			result->invalid_relationship_rows++;
			record_error(
				result,
				ORLIX_TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP);
			continue;
		}
		result->classified_rows++;
		switch (row->classification) {
		case ORLIX_TCTI_TARGET_COMPLETION_REQUIRED_EL0:
			result->required_el0_rows++;
			break;
		case ORLIX_TCTI_TARGET_COMPLETION_NON_EL0:
			result->non_el0_rows++;
			break;
		case ORLIX_TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED:
			result->undefined_or_unallocated_rows++;
			break;
		case ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE:
			result->alias_or_duplicate_rows++;
			break;
		case ORLIX_TCTI_TARGET_COMPLETION_UNCLASSIFIED:
			break;
		}

		relationship_valid =
			row->classification ==
				ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE ?
			valid_alias_relationship(
				source, source_count, classification,
				classification_count, index) :
			valid_non_alias_relationship(row);
		if (!relationship_valid) {
			result->invalid_relationship_rows++;
			record_error(
				result,
				ORLIX_TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP);
		}

		if (empty(row->evidence) || empty(row->proof_id)) {
			result->source_unbound_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
			continue;
		}
		if (row->classification ==
		    ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE) {
			size_t canonical_index =
				find_source(source, source_count,
					    row->canonical_name);
			size_t canonical_classification =
				find_classification(classification,
						    classification_count,
						    row->canonical_name);
			bool alias_binding_valid =
				registry_valid &&
				registry_binds_source(
					registry, registry_count,
					row->proof_id,
					&source[source_index]);

			if (!relationship_valid ||
			    canonical_index == source_count ||
			    canonical_classification ==
				    classification_count ||
			    !registry_valid ||
			    !source_proof_binding_resolves(
				    &source[canonical_index],
				    &classification[
					    canonical_classification],
				    registry, registry_count) ||
			    !alias_binding_valid) {
				result->source_unbound_rows++;
				record_error(
					result,
					ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
			} else {
				result->source_bound_rows++;
			}
		} else if (registry_valid &&
			   source_proof_binding_resolves(&source[source_index], row,
					  registry, registry_count)) {
			result->source_bound_rows++;
		} else {
			result->source_unbound_rows++;
			record_error(result,
				     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
		}
	}
	return result->errors ? -1 : 0;
}

int orlix_tcti_target_completion_validate_source_provenance(
	const struct orlix_tcti_target_completion_source_provenance *provenance,
	struct orlix_tcti_target_completion_result *result)
{
	if (!result)
		return -1;
	if (!provenance || empty(provenance->architecture) ||
	    empty(provenance->build) || empty(provenance->release) ||
	    empty(provenance->schema) || empty(provenance->timestamp) ||
	    empty(provenance->source_sha256) ||
	    strcmp(provenance->architecture, "vFATAp1-A") ||
	    strcmp(provenance->build, "818") ||
	    strcmp(provenance->release, "2026-06_rel") ||
	    strcmp(provenance->schema, "2.9.5") ||
	    strcmp(provenance->timestamp, "2026-06-24 17:12:14") ||
	    strcmp(provenance->source_sha256,
		   "a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe") ||
	    provenance->source_byte_length !=
		ORLIX_TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH ||
	    provenance->leaf_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS) {
		result->invalid_source_provenance++;
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_PROVENANCE);
		return -1;
	}
	return 0;
}

int orlix_tcti_target_completion_audit(struct orlix_tcti_target_completion_result *result)
{
	return completion_audit_internal(NULL, result, NULL);
}

const struct orlix_tcti_target_completion_source_provenance *
orlix_tcti_target_completion_source_provenance(void)
{
	return &source_provenance;
}

const struct orlix_tcti_target_completion_semantic_provenance *
orlix_tcti_target_completion_semantic_provenance(void)
{
	return &semantic_provenance;
}

const struct orlix_tcti_target_completion_semantic_provenance_row *
orlix_tcti_target_completion_semantic_provenance_rows(size_t *count)
{
	if (count)
		*count = sizeof(semantic_provenance_rows) /
			sizeof(semantic_provenance_rows[0]);
	return semantic_provenance_rows;
}

const struct orlix_tcti_target_completion_system_accessor_provenance *
orlix_tcti_target_completion_system_accessor_provenance(void)
{
	return &system_accessor_provenance;
}

const struct orlix_tcti_target_completion_system_accessor_row *
orlix_tcti_target_completion_system_accessors(size_t *count)
{
	if (count)
		*count =
			sizeof(system_accessor_rows) /
			sizeof(system_accessor_rows[0]);
	return system_accessor_rows;
}

const struct orlix_tcti_target_completion_source_row *
orlix_tcti_target_completion_source(size_t *count)
{
	if (count)
		*count = sizeof(source_rows) / sizeof(source_rows[0]);
	return source_rows;
}

const struct orlix_tcti_target_completion_classification_row *
orlix_tcti_target_completion_classification(size_t *count)
{
	if (count)
		*count =
			sizeof(classification_rows) /
			sizeof(classification_rows[0]);
	return classification_rows;
}

static const struct orlix_tcti_target_proof_registry_entry *
completion_registry_entry(const struct orlix_tcti_target_proof_registry_entry *registry,
				  size_t registry_count, const char *id)
{
	size_t index;

	if (empty(id))
		return NULL;
	for (index = 0; index < registry_count; index++)
		if (!strcmp(registry[index].id, id))
			return &registry[index];
	return NULL;
}

static bool completion_alias_proof_resolves(
	size_t ordinal, const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count)
{
	const struct orlix_tcti_target_completion_classification_row *row =
		&classification_rows[ordinal];
	size_t canonical_source;
	size_t canonical_classification;

	if (!valid_alias_relationship(source_rows,
				      sizeof(source_rows) / sizeof(source_rows[0]),
				      classification_rows,
				      sizeof(classification_rows) / sizeof(classification_rows[0]),
				      ordinal) ||
	    !registry_binds_source(registry, registry_count, row->proof_id,
				   &source_rows[ordinal]))
		return false;
	canonical_source = find_source(source_rows,
				       sizeof(source_rows) / sizeof(source_rows[0]),
				       row->canonical_name);
	canonical_classification = find_classification(classification_rows,
						 sizeof(classification_rows) /
						 sizeof(classification_rows[0]),
						 row->canonical_name);
	return canonical_source < sizeof(source_rows) / sizeof(source_rows[0]) &&
		canonical_classification < sizeof(classification_rows) /
			sizeof(classification_rows[0]) &&
		source_proof_binding_resolves(&source_rows[canonical_source],
					     &classification_rows[canonical_classification],
					     registry, registry_count);
}

static void completion_project_obligation(
	size_t ordinal, const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count,
	const struct orlix_tcti_runtime_capability_cohort_artifact *runtime,
	struct orlix_tcti_target_completion_obligation *obligation)
{
	const struct orlix_tcti_target_completion_source_row *source = &source_rows[ordinal];
	const struct orlix_tcti_target_completion_classification_row *classification =
		&classification_rows[ordinal];
	const struct orlix_tcti_target_completion_semantic_provenance_row *semantic =
		&semantic_provenance_rows[ordinal];
	const struct orlix_tcti_target_proof_registry_entry *entry;
	const struct orlix_tcti_runtime_capability_cohort_leaf *runtime_leaf;
	bool proof_resolves = false;

	memset(obligation, 0, sizeof(*obligation));
	obligation->ordinal = source->ordinal;
	obligation->name = source->name;
	obligation->mnemonic = source->mnemonic;
	obligation->operation_id = source->operation_id;
	obligation->classification = classification->classification;
	obligation->relation = classification->relation;
	obligation->canonical_name = classification->canonical_name;
	obligation->semantic_provenance_disposition = semantic->disposition;
	obligation->semantic_provenance_row = semantic;
	obligation->semantic_relative_file = semantic->relative_file;
	obligation->semantic_decode_locator = semantic->decode_locator;
	obligation->semantic_decode_sha256 = semantic->decode_sha256;
	obligation->semantic_execute_locator = semantic->execute_locator;
	obligation->semantic_execute_sha256 = semantic->execute_sha256;
	obligation->aarchmrs_operation_locator =
		semantic->aarchmrs_operation_locator;
	obligation->aarchmrs_operation_sha256 =
		semantic->aarchmrs_operation_sha256;
	obligation->proof_id = classification->proof_id;

	if (classification->classification ==
	    ORLIX_TCTI_TARGET_COMPLETION_UNCLASSIFIED) {
		obligation->blocker_mask |=
			ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_UNCLASSIFIED;
	} else if (classification->classification ==
		   ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE ?
		   !valid_alias_relationship(source_rows,
				     sizeof(source_rows) / sizeof(source_rows[0]),
				     classification_rows,
				     sizeof(classification_rows) /
				     sizeof(classification_rows[0]), ordinal) :
		   !valid_non_alias_relationship(classification)) {
		obligation->blocker_mask |=
			ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_RELATIONSHIP;
	}

	if (semantic->disposition ==
	    ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_OFFICIAL_SEMANTICS_NOT_SPECIFIED)
		obligation->blocker_mask |=
			ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_SEMANTIC_PROVENANCE;

	entry = completion_registry_entry(registry, registry_count,
					 obligation->proof_id);
	if (!entry) {
		obligation->proof_state = ORLIX_TCTI_TARGET_COMPLETION_PROOF_NONE;
		obligation->blocker_mask |= ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_PROOF;
	} else {
		obligation->required_obligations = entry->obligations;
		obligation->unproved_obligations = entry->unproved_obligations;
		obligation->kunit_source = entry->kunit_source;
		obligation->kunit_suite = entry->kunit_suite;
		obligation->kselftest = entry->kselftest;
		/* Registry provenance never observes native test execution. */
		obligation->blocker_mask |=
			ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_EXECUTION_EVIDENCE;
		if (classification->classification ==
		    ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE)
			proof_resolves = completion_alias_proof_resolves(ordinal,
								 registry, registry_count);
		else
			proof_resolves = source_proof_binding_resolves(source,
				classification, registry, registry_count);
		if (!proof_resolves) {
			obligation->proof_state = ORLIX_TCTI_TARGET_COMPLETION_PROOF_STALE;
			obligation->blocker_mask |=
				ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_PROOF;
		} else if (entry->unproved_obligations) {
			obligation->proof_state =
				ORLIX_TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_UNPROVED;
			obligation->blocker_mask |=
				ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_UNPROVED_OBLIGATIONS;
	} else {
		obligation->proof_state =
			ORLIX_TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_NO_UNPROVED_METADATA;
	}
	}

	runtime_leaf = &runtime->leaves[ordinal];
	obligation->runtime_candidate_first = runtime_leaf->first_membership;
	obligation->runtime_candidate_count = runtime_leaf->membership_count;
	if (runtime_leaf->membership_count) {
		obligation->runtime_candidate_state =
			ORLIX_TCTI_TARGET_COMPLETION_RUNTIME_CANDIDATE_UNRESOLVED;
		obligation->blocker_mask |=
			ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_RUNTIME_CANDIDATE;
	} else {
		obligation->runtime_candidate_state =
			ORLIX_TCTI_TARGET_COMPLETION_RUNTIME_CANDIDATE_NONE;
	}
}

static int completion_validate_linux_proof_matrix(
	const struct orlix_tcti_target_linux_proof_disposition_row *rows,
	size_t count, struct orlix_tcti_target_completion_result *result)
{
	struct orlix_tcti_target_linux_proof_matrix_result matrix;
	int status;

	status = orlix_tcti_target_linux_proof_matrix_validate(rows, count, &matrix);
	result->linux_proof_rows = matrix.total_rows;
	result->linux_proof_source_leaf_rows = matrix.source_leaf_rows;
	result->linux_proof_semantic_variant_rows = matrix.semantic_variant_rows;
	result->linux_proof_kselftest_owned_rows = matrix.kselftest_owned_rows;
	result->linux_proof_not_applicable_rows = matrix.not_applicable_rows;
	result->linux_proof_executed_rows = matrix.executed_kselftest_rows;
	result->missing_linux_proof_rows = matrix.missing_rows;
	result->duplicate_linux_proof_rows = matrix.duplicate_rows;
	result->stale_linux_proof_rows = matrix.stale_rows;
	result->malformed_linux_proof_rows = matrix.malformed_rows;
	result->ambiguous_linux_proof_rows = matrix.ambiguous_rows;
	result->invalid_linux_proof_provenance_rows =
		matrix.invalid_provenance_rows;
	result->linux_proof_substitution_rows = matrix.substitution_rows;
	if (status)
		record_error(result,
			     ORLIX_TCTI_TARGET_COMPLETION_ERROR_LINUX_PROOF_MATRIX);
	return status;
}

static int completion_audit_internal(
	const struct orlix_tcti_target_completion_audit_inputs_for_test *inputs,
	struct orlix_tcti_target_completion_result *result,
	struct orlix_tcti_target_completion_obligation *obligations)
{
	const struct orlix_tcti_target_proof_registry_entry *registry;
	const struct orlix_tcti_target_operational_note_proof_mapping
		*operational_note_mappings;
	const struct orlix_tcti_runtime_capability_cohort_artifact *runtime;
	const struct orlix_tcti_feature_artifact *feature_artifact =
		orlix_tcti_feature_artifact_canonical();
	const struct orlix_tcti_target_instruction_artifact *instruction_artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_target_feature_applicability_artifact
		*feature_applicability =
			orlix_tcti_target_feature_applicability_artifact();
	const struct orlix_tcti_target_linux_proof_disposition_row *linux_proof;
	size_t linux_proof_count;
	const struct orlix_tcti_target_completion_semantic_provenance
		*checked_semantic_provenance = &semantic_provenance;
	const struct orlix_tcti_target_completion_semantic_provenance_row
		*checked_semantic_provenance_rows = semantic_provenance_rows;
	size_t checked_semantic_provenance_count =
		sizeof(semantic_provenance_rows) /
		sizeof(semantic_provenance_rows[0]);
	struct orlix_tcti_runtime_capability_cohort_validation_result runtime_diagnostic;
	struct orlix_tcti_target_operational_note_mapping_result
		operational_note_mapping_result;
	int status;
	size_t registry_count;
	size_t operational_note_mapping_count;
	size_t index;
	const struct orlix_tcti_target_completion_source_row *source = source_rows;
	const struct orlix_tcti_target_completion_classification_row *classification =
		classification_rows;
	size_t source_count = sizeof(source_rows) / sizeof(source_rows[0]);
	size_t classification_count =
		sizeof(classification_rows) / sizeof(classification_rows[0]);

	if (!result)
		return -1;
	/* Validate before touching caller output, so malformed runtime input is atomic. */
	runtime = orlix_tcti_runtime_capability_cohort_artifact_canonical();
	if (!runtime || orlix_tcti_runtime_capability_cohort_artifact_validate(
			    runtime, &runtime_diagnostic))
		return -1;
	registry = orlix_tcti_target_proof_registry_entries(&registry_count);
	operational_note_mappings =
		orlix_tcti_target_operational_note_proof_mappings(
			&operational_note_mapping_count);
	linux_proof = orlix_tcti_target_linux_proof_dispositions(&linux_proof_count);
	if (inputs) {
		source = inputs->source;
		source_count = inputs->source_count;
		classification = inputs->classification;
		classification_count = inputs->classification_count;
		registry = inputs->registry;
		registry_count = inputs->registry_count;
		operational_note_mappings = inputs->operational_note_mappings;
		operational_note_mapping_count =
			inputs->operational_note_mapping_count;
		if (inputs->instruction_artifact)
			instruction_artifact = inputs->instruction_artifact;
		if (inputs->feature_applicability)
			feature_applicability = inputs->feature_applicability;
		if (inputs->linux_proof || inputs->linux_proof_count) {
			linux_proof = inputs->linux_proof;
			linux_proof_count = inputs->linux_proof_count;
		}
		if (inputs->semantic_provenance ||
		    inputs->semantic_provenance_rows ||
		    inputs->semantic_provenance_count) {
			checked_semantic_provenance = inputs->semantic_provenance;
			checked_semantic_provenance_rows =
				inputs->semantic_provenance_rows;
			checked_semantic_provenance_count =
				inputs->semantic_provenance_count;
		}
	}
	/*
	 * Reject malformed injected dependencies before constructing staged rows.
	 * Ordinary completion gaps stay red but remain safe to project.
	 */
	status = orlix_tcti_target_completion_validate(
		source, source_count, classification, classification_count,
		registry, registry_count, result);
	if (!source || !classification)
		return -1;
	if (!validate_completion_feature_dependencies(feature_artifact,
					      instruction_artifact, source_count, result))
		return -1;
	result->operational_note_rows = instruction_artifact->operational_note_count;
	if (orlix_tcti_target_operational_note_proof_mappings_validate(
		instruction_artifact, operational_note_mappings,
		operational_note_mapping_count, registry, registry_count,
		&operational_note_mapping_result)) {
		result->invalid_operational_note_mappings = 1U;
		record_error(result,
			ORLIX_TCTI_TARGET_COMPLETION_ERROR_OPERATIONAL_NOTE);
		status = -1;
	} else {
		result->mapped_operational_note_rows =
			operational_note_mapping_result.mapped_count;
	}
	/* Static mapping cannot grant native execution credit. */
	result->unproved_operational_note_rows =
		instruction_artifact->operational_note_count;
	if (result->unproved_operational_note_rows) {
		record_error(result,
			ORLIX_TCTI_TARGET_COMPLETION_ERROR_OPERATIONAL_NOTE);
		status = -1;
	}
	if (!completion_projection_dependencies_valid(result) ||
	    source_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS ||
	    classification_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS)
		return -1;
	if (obligations)
		for (index = 0; index < ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS; index++)
			completion_project_obligation(index, registry, registry_count,
					      runtime, &obligations[index]);
	if (orlix_tcti_target_completion_validate_source_provenance(
		    &source_provenance, result))
		status = -1;
	if (orlix_tcti_target_completion_validate_semantic_provenance(
		    source, source_count, checked_semantic_provenance,
		    checked_semantic_provenance_rows,
		    checked_semantic_provenance_count, result))
		status = -1;
	if (orlix_tcti_target_completion_validate_system_accessors(
		    source, source_count,
		    &system_accessor_provenance, system_accessor_rows,
		    sizeof(system_accessor_rows) / sizeof(system_accessor_rows[0]),
		    result))
		status = -1;
	if (orlix_tcti_target_completion_validate_feature_field_domains(
		    orlix_tcti_feature_artifact_canonical(),
		    orlix_tcti_feature_field_domain_binding_artifact_canonical(), result))
		status = -1;
	if (orlix_tcti_target_completion_validate_runtime_capability_cohorts(runtime,
								     result))
		status = -1;
	if (completion_validate_linux_proof_matrix(linux_proof, linux_proof_count,
						   result))
		status = -1;
	validate_source_feature_domain(source, source_count, feature_artifact,
				       instruction_artifact, feature_applicability,
				       result, obligations);
	if (result->invalid_feature_artifact ||
	    result->invalid_source_condition_rows ||
	    result->unresolved_feature_applicability_rows)
		status = -1;
	return status;
}

int orlix_tcti_target_completion_audit_with_obligations(
	struct orlix_tcti_target_completion_result *result,
	struct orlix_tcti_target_completion_obligation *obligations,
	size_t obligation_count)
{
	if (!result || (obligations &&
			obligation_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS) ||
		(!obligations && obligation_count))
		return -1;
	return orlix_tcti_target_completion_audit_with_inputs_for_test(
		NULL, result, obligations, obligation_count);
}

int orlix_tcti_target_completion_audit_with_inputs_for_test(
	const struct orlix_tcti_target_completion_audit_inputs_for_test *inputs,
	struct orlix_tcti_target_completion_result *result,
	struct orlix_tcti_target_completion_obligation *obligations,
	size_t obligation_count)
{
	struct orlix_tcti_target_completion_audit_inputs_for_test canonical_inputs;
	const struct orlix_tcti_target_completion_audit_inputs_for_test *effective =
		inputs;
	struct orlix_tcti_target_completion_obligation *staging;
	int status;
	size_t registry_count;
	size_t operational_note_mapping_count;

	if (!result || (obligations &&
			obligation_count != ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS) ||
		(!obligations && obligation_count))
		return -1;
	if (!effective) {
		canonical_inputs =
			(struct orlix_tcti_target_completion_audit_inputs_for_test) {
				.source = source_rows,
				.source_count = sizeof(source_rows) / sizeof(source_rows[0]),
				.classification = classification_rows,
				.classification_count = sizeof(classification_rows) /
					sizeof(classification_rows[0]),
				.registry = orlix_tcti_target_proof_registry_entries(&registry_count),
				.registry_count = registry_count,
				.operational_note_mappings =
					orlix_tcti_target_operational_note_proof_mappings(
						&operational_note_mapping_count),
				.operational_note_mapping_count =
					operational_note_mapping_count,
			};
		effective = &canonical_inputs;
	}
	if (!obligations)
		return completion_audit_internal(effective, result, NULL);
	staging = calloc(ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS, sizeof(*staging));
	if (!staging)
		return -1;
	status = completion_audit_internal(effective, result, staging);
	/* Completion blockers are expected. Malformed dependencies are not. */
	if (completion_projection_dependencies_valid(result))
		memcpy(obligations, staging,
		       ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS * sizeof(*staging));
	free(staging);
	return status;
}
