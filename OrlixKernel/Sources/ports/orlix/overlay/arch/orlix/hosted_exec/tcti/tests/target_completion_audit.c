/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_completion_audit.h"
#include "target_feature_artifact.h"
#include "target_feature_domain.h"
#include "target_instruction_artifact.h"

#include <stdbool.h>
#include <string.h>

#define stringify_1(value) #value
#define stringify(value) stringify_1(value)

#define UNCLASSIFIED TCTI_TARGET_COMPLETION_UNCLASSIFIED
#define REQUIRED TCTI_TARGET_COMPLETION_REQUIRED_EL0
#define REQUIRED_EL0 TCTI_TARGET_COMPLETION_REQUIRED_EL0
#define NON_EL0 TCTI_TARGET_COMPLETION_NON_EL0
#define ARCHITECTURALLY_UNDEFINED \
	TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED
#define ARCH_UNDEFINED_OR_UNALLOCATED \
	TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED
#define ALIAS_OR_DUPLICATE TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE
#define TCTI_A64_TARGET_RELATION_NONE TCTI_TARGET_COMPLETION_RELATION_NONE
#define TCTI_A64_TARGET_RELATION_ALIAS TCTI_TARGET_COMPLETION_RELATION_ALIAS
#define TCTI_A64_TARGET_RELATION_DUPLICATE \
	TCTI_TARGET_COMPLETION_RELATION_DUPLICATE

static const struct tcti_target_completion_source_provenance source_provenance = {
#define TCTI_A64_SOURCE_MANIFEST_SOURCE(arch_value, build_value, release_value, \
					schema_value, sha_value, count_value, \
					timestamp_value, source_length_value) \
	arch_value, build_value, release_value, schema_value, timestamp_value, \
	sha_value, source_length_value, count_value
#define TCTI_A64_SOURCE_MANIFEST_ROW(...)
#include "../isa/source_manifest.def"
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, \
				      pattern, condition, source_offset, source_length) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition, \
	  source_offset, source_length },
static const struct tcti_target_completion_source_row source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct tcti_target_completion_asl_provenance asl_provenance = {
#define TCTI_A64_ASL_AVAILABILITY_SOURCE(format, source_sha256, availability) \
	format, source_sha256, availability
#define TCTI_A64_ASL_AVAILABILITY_ROW(...)
#include "../isa/target_asl_availability.def"
#undef TCTI_A64_ASL_AVAILABILITY_ROW
#undef TCTI_A64_ASL_AVAILABILITY_SOURCE
};

#define TCTI_A64_ASL_AVAILABILITY_SOURCE(...)
#define TCTI_A64_ASL_AVAILABILITY_ROW(ordinal, name, operation, object, \
					      offset, length, note_presence, note_offset, \
					      note_length, note_digest, availability) \
	{ ordinal, name, operation, object, offset, length, note_presence, \
	  note_offset, note_length, note_digest, availability },
static const struct tcti_target_completion_asl_row asl_rows[] = {
#include "../isa/target_asl_availability.def"
};
#undef TCTI_A64_ASL_AVAILABILITY_ROW
#undef TCTI_A64_ASL_AVAILABILITY_SOURCE

static const struct tcti_target_completion_system_accessor_provenance
system_accessor_provenance = {
#define TCTI_A64_SYSTEM_ACCESSOR_SOURCE(architecture, build, release, schema, \
					timestamp, sha256) \
	architecture, build, release, schema, timestamp, sha256,
#define TCTI_A64_SYSTEM_ACCESSOR_COUNTS(total, mapped, reserved, privileged, \
					unsupported, ambiguous, contradictory, \
					invalid) \
	total, mapped, reserved, privileged, unsupported, ambiguous, \
	contradictory, invalid,
#define TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(identity) identity,
#define TCTI_A64_SYSTEM_ACCESSOR(...)
#include "../isa/target_system_accessor_reconciliation.def"
#undef TCTI_A64_SYSTEM_ACCESSOR
#undef TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef TCTI_A64_SYSTEM_ACCESSOR_SOURCE
};

#define TCTI_A64_SYSTEM_ACCESSOR_SOURCE(...)
#define TCTI_A64_SYSTEM_ACCESSOR_COUNTS(...)
#define TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(...)
#define TCTI_A64_SYSTEM_ACCESSOR(accessor, encoding, name, generic, direction, \
				 disposition, selectors, condition, \
				 selector_identity, condition_identity, \
				 accessor_offset, accessor_length, \
				 encoding_offset, encoding_length, condition_offset, \
				 condition_length) \
	{ accessor, encoding, name, generic, direction, disposition, selectors, \
	  condition, selector_identity, condition_identity, accessor_offset, \
	  accessor_length, encoding_offset, encoding_length, condition_offset, \
	  condition_length },
static const struct tcti_target_completion_system_accessor_row
system_accessor_rows[] = {
#include "../isa/target_system_accessor_reconciliation.def"
};
#undef TCTI_A64_SYSTEM_ACCESSOR
#undef TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef TCTI_A64_SYSTEM_ACCESSOR_SOURCE

#define TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
				       canonical, evidence, proof) \
	{ stringify(name), classification, relation, canonical, evidence, proof },
static const struct tcti_target_completion_classification_row
classification_rows[] = {
#include "../isa/target_classification.def"
};
#undef TCTI_A64_TARGET_CLASSIFICATION

_Static_assert(sizeof(source_rows) / sizeof(source_rows[0]) ==
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS,
	       "source manifest must contain exactly 4,350 rows");
_Static_assert(sizeof(classification_rows) / sizeof(classification_rows[0]) ==
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS,
	       "classification ledger must contain exactly 4,350 rows");
_Static_assert(sizeof(asl_rows) / sizeof(asl_rows[0]) ==
		       TCTI_TARGET_COMPLETION_SOURCE_ROWS,
	       "ASL availability ledger must contain exactly 4,350 rows");
_Static_assert(sizeof(system_accessor_rows) / sizeof(system_accessor_rows[0]) ==
		       TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS,
	       "system accessor ledger must contain exactly 2,014 rows");

static bool empty(const char *text)
{
	return !text || !text[0];
}

static void record_error(struct tcti_target_completion_result *result,
			 uint32_t error);

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
	const struct tcti_target_completion_source_row *source,
	const struct tcti_target_instruction_artifact_leaf *leaf,
	const struct tcti_target_instruction_artifact *artifact)
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

static void validate_source_feature_domain(
	const struct tcti_target_completion_source_row *source,
	size_t source_count, struct tcti_target_completion_result *result)
{
	const struct tcti_feature_artifact *feature_artifact =
		tcti_feature_artifact_canonical();
	const struct tcti_target_instruction_artifact *instruction_artifact =
		tcti_target_instruction_artifact_canonical();
	struct tcti_feature_artifact_scratch feature_scratch;
	struct tcti_feature_artifact_diagnostic feature_diagnostic;
	struct tcti_target_instruction_artifact_validation_result
		instruction_diagnostic;
	static tcti_feature_artifact_u8 feature_state[
		TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static struct tcti_feature_artifact_frame feature_frames[
		TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static tcti_feature_artifact_u8 feature_child_coverage[
		TCTI_FEATURE_ARTIFACT_CHILD_COUNT];
	size_t index;

	feature_scratch = (struct tcti_feature_artifact_scratch) {
		.state = feature_state,
		.state_count = sizeof(feature_state),
		.frames = feature_frames,
		.frame_count = sizeof(feature_frames) / sizeof(feature_frames[0]),
		.child_coverage = feature_child_coverage,
		.child_coverage_count = sizeof(feature_child_coverage),
	};
	if (tcti_feature_artifact_validate(feature_artifact, &feature_scratch,
					  &feature_diagnostic) !=
		TCTI_FEATURE_ARTIFACT_VALID ||
	    tcti_target_instruction_artifact_validate(instruction_artifact,
						      &instruction_diagnostic) ||
	    instruction_artifact->leaf_count != source_count) {
		result->invalid_feature_artifact++;
		record_error(result, TCTI_TARGET_COMPLETION_ERROR_FEATURE_DOMAIN);
		return;
	}
	for (index = 0; index < source_count; index++) {
		struct tcti_feature_domain_tcnd_diagnostic diagnostic;
		const struct tcti_target_instruction_artifact_leaf *leaf =
			&instruction_artifact->leaves[index];

		if (!source_condition_matches_artifact(&source[index], leaf,
						      instruction_artifact) ||
		    tcti_feature_domain_validate_tcnd_features(
				feature_artifact, source[index].condition_tcnd_hex,
				&diagnostic)) {
			result->invalid_source_condition_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_FEATURE_DOMAIN);
			continue;
		}
		result->source_condition_domain_bound_rows++;
		/*
		 * Structural binding is deliberately not treated as a SAT witness.
		 * This must remain a hard blocker until the build-time C-native
		 * feature/operand union artifact supplies one exact result per leaf.
		 */
		result->unresolved_feature_applicability_rows++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_FEATURE_APPLICABILITY);
	}
}

int tcti_target_completion_validate_feature_field_domains(
	const struct tcti_feature_artifact *feature_artifact,
	const struct tcti_feature_field_domain_binding_artifact *artifact,
	struct tcti_target_completion_result *result)
{
	struct tcti_feature_field_domain_binding_scratch scratch;
	struct tcti_feature_field_domain_binding_diagnostic diagnostic;
	static tcti_feature_artifact_u8 feature_node_coverage[
		TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static tcti_feature_artifact_u32 identity_group_coverage[
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT];
	tcti_feature_artifact_u32 index;

	if (!result)
		return -1;
	scratch = (struct tcti_feature_field_domain_binding_scratch) {
		.feature_node_coverage = feature_node_coverage,
		.feature_node_coverage_count = sizeof(feature_node_coverage),
		.identity_group_coverage = identity_group_coverage,
		.identity_group_coverage_count =
			sizeof(identity_group_coverage) /
			sizeof(identity_group_coverage[0]),
	};
	if (tcti_feature_field_domain_binding_validate(feature_artifact, artifact,
						       &scratch, &diagnostic) !=
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID) {
		result->invalid_feature_field_domain_rows++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
		return -1;
	}
	for (index = 0; index < artifact->occurrence_count; index++) {
		const struct tcti_feature_field_domain_binding *binding =
			&artifact->bindings[index];

		result->feature_field_domain_rows++;
		if (binding->disposition == TCTI_FEATURE_FIELD_DOMAIN_MAPPED) {
			result->mapped_feature_field_domain_rows++;
			/*
			 * A source span and a Registers.json value-relation index are
			 * ownership, not satisfiability or execution proof. Keep the
			 * entire mapped domain blocking until the typed evaluator owns
			 * every relation.
			 */
			result->unresolved_feature_field_domain_rows++;
		} else {
			result->ambiguous_feature_field_domain_rows++;
		}
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN);
	}
	return -1;
}

int tcti_target_completion_validate_runtime_capability_cohorts(
	const struct tcti_runtime_capability_cohort_artifact *artifact,
	struct tcti_target_completion_result *result)
{
	struct tcti_runtime_capability_cohort_validation_result diagnostic;
	size_t leaf_index;

	if (!result)
		return -1;
	if (tcti_runtime_capability_cohort_artifact_validate(artifact,
						     &diagnostic)) {
		result->invalid_runtime_capability_cohort_rows++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT);
		return -1;
	}
	for (leaf_index = 0; leaf_index < artifact->counts.leaf_count;
	     leaf_index++) {
		const struct tcti_runtime_capability_cohort_leaf *leaf =
			&artifact->leaves[leaf_index];
		size_t membership_index;

		result->runtime_capability_cohort_leaf_rows++;
		for (membership_index = leaf->first_membership;
		     membership_index < leaf->first_membership + leaf->membership_count;
		     membership_index++) {
			const struct tcti_runtime_capability_cohort_membership *member =
				&artifact->memberships[membership_index];

			result->runtime_capability_cohort_candidate_membership_rows++;
			if (member->disposition ==
			    TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED) {
				result->unresolved_runtime_capability_cohort_membership_rows++;
				record_error(result,
					     TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT);
			}
		}
	}
	return result->unresolved_runtime_capability_cohort_membership_rows ?
		-1 : 0;
}

static bool source_row_well_formed(
	const struct tcti_target_completion_source_row *row, size_t ordinal)
{
	return row && row->ordinal == ordinal && !empty(row->name) &&
		!empty(row->mnemonic) && !empty(row->operation_id) &&
		!empty(row->condition_tcnd_hex) && row->source_length &&
		row->source_offset < TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH &&
		row->source_length <=
			TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH - row->source_offset;
}

static bool source_row_matches_canonical(
	const struct tcti_target_completion_source_row *row, size_t ordinal)
{
	const struct tcti_target_completion_source_row *canonical;

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

static bool asl_availability_is_valid(const char *availability)
{
	/*
	 * A future available state must add a corpus digest plus an ASL entry and
	 * body locator to this checked schema.  Do not accept a status-string-only
	 * claim before that provenance exists.
	 */
	return availability && !strcmp(availability, "shared_asl_absent_blocking");
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

static bool asl_operational_note_provenance_is_valid(
	const struct tcti_target_completion_asl_row *row)
{
	if (!row || empty(row->operational_note_presence) ||
	    !row->operational_note_sha256)
		return false;
	if (!strcmp(row->operational_note_presence, "absent"))
		return row->operational_note_source_offset == 0U &&
			row->operational_note_source_length == 0U &&
			row->operational_note_sha256[0] == '\0';
	if (strcmp(row->operational_note_presence, "present"))
		return false;
	return row->operational_note_source_length != 0U &&
		row->operational_note_source_offset <
			TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH &&
		row->operational_note_source_length <=
			TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH -
				row->operational_note_source_offset &&
		sha256_hex_is_valid(row->operational_note_sha256);
}

int tcti_target_completion_validate_asl_availability(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_asl_provenance *provenance,
	const struct tcti_target_completion_asl_row *availability,
	size_t availability_count,
	struct tcti_target_completion_result *result)
{
	size_t index;

	if (!result)
		return -1;
	if (!source || source_count != TCTI_TARGET_COMPLETION_SOURCE_ROWS ||
	    !availability ||
	    availability_count != TCTI_TARGET_COMPLETION_SOURCE_ROWS ||
	    !provenance || empty(provenance->format) ||
	    empty(provenance->source_sha256) || empty(provenance->availability) ||
	    strcmp(provenance->format, "inline_aarchmrs_operations_v2") ||
	    strcmp(provenance->source_sha256,
		   "a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe") ||
	    !asl_availability_is_valid(provenance->availability)) {
		record_error(result, TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
		if (!source || !availability)
			return -1;
	}

	for (index = 0; index < source_count; index++) {
		const struct tcti_target_completion_asl_row *row;
		const struct tcti_target_completion_asl_row *canonical;
		bool valid;

		if (index >= availability_count) {
			result->invalid_asl_availability_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
			continue;
		}
		row = &availability[index];
		/* The checked artifact is the source-derived absence witness. */
		canonical = &asl_rows[index];
		valid = source_row_well_formed(&source[index], index) &&
			row->ordinal == index && !empty(row->name) &&
			!empty(row->operation_id) && !empty(row->operation_object) &&
			row->source_length != 0U &&
			row->source_offset <
				TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH &&
			row->source_length <=
				TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH -
					row->source_offset &&
			asl_operational_note_provenance_is_valid(row) &&
			asl_availability_is_valid(row->availability) &&
			!strcmp(row->name, source[index].name) &&
			!strcmp(row->operation_id, source[index].operation_id) &&
			row->ordinal == canonical->ordinal &&
			!strcmp(row->name, canonical->name) &&
			!strcmp(row->operation_id, canonical->operation_id) &&
			!strcmp(row->operation_object, canonical->operation_object) &&
			row->source_offset == canonical->source_offset &&
			row->source_length == canonical->source_length &&
			!strcmp(row->operational_note_presence,
				canonical->operational_note_presence) &&
			row->operational_note_source_offset ==
				canonical->operational_note_source_offset &&
			row->operational_note_source_length ==
				canonical->operational_note_source_length &&
			!strcmp(row->operational_note_sha256,
				canonical->operational_note_sha256) &&
			!strcmp(row->availability, canonical->availability);
		if (!valid) {
			result->invalid_asl_availability_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
			continue;
		}
		result->asl_availability_rows++;
		if (!strcmp(row->availability, "shared_asl_absent_blocking")) {
			result->unavailable_asl_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY);
		}
	}
	return result->invalid_asl_availability_rows ||
		result->unavailable_asl_rows ? -1 : 0;
}

static void record_error(struct tcti_target_completion_result *result,
			 uint32_t error)
{
	result->error_mask |= error;
	result->errors++;
}

static size_t find_source(
	const struct tcti_target_completion_source_row *source,
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

static bool register_source_span_valid(uint64_t offset, uint64_t length)
{
	return length && offset < TCTI_TARGET_COMPLETION_REGISTERS_BYTE_LENGTH &&
	       length <= TCTI_TARGET_COMPLETION_REGISTERS_BYTE_LENGTH - offset;
}

static uint64_t accessor_identity_byte(uint64_t identity, unsigned char byte)
{
	return (identity ^ byte) * UINT64_C(1099511628211);
}

static uint64_t accessor_identity_u64(uint64_t identity, uint64_t value)
{
	unsigned int index;

	for (index = 0; index < 8U; index++)
		identity = accessor_identity_byte(identity,
			(unsigned char)(value >> (index * 8U)));
	return identity;
}

static uint64_t accessor_identity_text(uint64_t identity, const char *text)
{
	size_t length = strlen(text);
	size_t index;

	identity = accessor_identity_u64(identity, length);
	for (index = 0; index < length; index++)
		identity = accessor_identity_byte(identity, (unsigned char)text[index]);
	return identity;
}

static uint64_t system_accessor_identity(
	const struct tcti_target_completion_system_accessor_row *accessors,
	size_t accessor_count)
{
	uint64_t identity = UINT64_C(1469598103934665603);
	size_t index;

	identity = accessor_identity_u64(identity, accessor_count);
	for (index = 0; index < accessor_count; index++) {
		const struct tcti_target_completion_system_accessor_row *row =
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

int tcti_target_completion_validate_system_accessors(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_system_accessor_provenance *provenance,
	const struct tcti_target_completion_system_accessor_row *accessors,
	size_t accessor_count,
	struct tcti_target_completion_result *result)
{
	size_t actual[7] = { 0 };
	size_t index;

	if (!result)
		return -1;
	result->system_accessor_rows = accessor_count;
	if (!source || source_count != TCTI_TARGET_COMPLETION_SOURCE_ROWS ||
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
		    TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS ||
	    accessor_count != TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS ||
	    provenance->mapped_count + provenance->reserved_count +
			    provenance->privileged_count +
			    provenance->unsupported_count +
			    provenance->ambiguous_count +
			    provenance->contradictory_count +
			    provenance->invalid_count !=
		    provenance->accessor_count) {
		result->invalid_system_accessor_rows++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
		if (!source || !provenance || !accessors)
			return -1;
	}

	for (index = 0; index < accessor_count; index++) {
		const struct tcti_target_completion_system_accessor_row *row =
			&accessors[index];
		size_t previous;
		bool valid = !empty(row->name) &&
			!strncmp(row->name, "A64.", 4U) &&
			row->direction >=
				TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_READ &&
			row->direction <=
				TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_EXECUTE &&
			row->disposition >=
				TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED &&
			row->disposition <=
				TCTI_TARGET_COMPLETION_ACCESSOR_INVALID &&
			register_source_span_valid(row->accessor_source_offset,
						   row->accessor_source_length);

		if (valid && row->disposition ==
				     TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED)
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
				     TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
			continue;
		}
		actual[row->disposition]++;
		if (row->disposition ==
		    TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED) {
			result->mapped_system_accessor_rows++;
		} else {
			result->nonmapped_system_accessor_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
		}
	}
	if (actual[TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED] !=
		    provenance->mapped_count ||
	    actual[TCTI_TARGET_COMPLETION_ACCESSOR_RESERVED] !=
		    provenance->reserved_count ||
	    actual[TCTI_TARGET_COMPLETION_ACCESSOR_PRIVILEGED] !=
		    provenance->privileged_count ||
	    actual[TCTI_TARGET_COMPLETION_ACCESSOR_UNSUPPORTED] !=
		    provenance->unsupported_count ||
	    actual[TCTI_TARGET_COMPLETION_ACCESSOR_AMBIGUOUS] !=
		    provenance->ambiguous_count ||
	    actual[TCTI_TARGET_COMPLETION_ACCESSOR_CONTRADICTORY] !=
		    provenance->contradictory_count ||
	    actual[TCTI_TARGET_COMPLETION_ACCESSOR_INVALID] !=
	    provenance->invalid_count) {
		result->invalid_system_accessor_rows++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
	}
	if (system_accessor_identity(accessors, accessor_count) !=
		    provenance->reconciliation_identity) {
		result->invalid_system_accessor_rows++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR);
	}
	return result->invalid_system_accessor_rows ||
		       result->nonmapped_system_accessor_rows ?
	       -1 : 0;
}

static size_t find_classification(
	const struct tcti_target_completion_classification_row *classification,
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
	const struct tcti_target_completion_source_row *source,
	const struct tcti_target_proof_binding *binding)
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
	const struct tcti_target_proof_registry_entry *registry,
	size_t registry_count, const char *proof_id,
	const struct tcti_target_completion_source_row *source)
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
	const struct tcti_target_completion_classification_row *row)
{
	return row->relation == TCTI_TARGET_COMPLETION_RELATION_NONE &&
	       empty(row->canonical_name);
}

static bool valid_alias_relationship(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_classification_row *classification,
	size_t classification_count, size_t row_index)
{
	const struct tcti_target_completion_classification_row *row =
		&classification[row_index];
	size_t source_index;
	size_t canonical_classification;
	size_t canonical_source;

	if (row->relation != TCTI_TARGET_COMPLETION_RELATION_ALIAS &&
	    row->relation != TCTI_TARGET_COMPLETION_RELATION_DUPLICATE)
		return false;
	if (empty(row->canonical_name) ||
	    !strcmp(row->name, row->canonical_name))
		return false;
	source_index = find_source(source, source_count, row->name);
	if (source_index == source_count)
		return false;
	canonical_classification = find_classification(
		classification, classification_count, row->canonical_name);
	canonical_source = find_source(source, source_count,
				       row->canonical_name);
	if (canonical_classification == classification_count ||
	    canonical_source == source_count ||
	    classification[canonical_classification].classification ==
		    TCTI_TARGET_COMPLETION_UNCLASSIFIED ||
	    classification[canonical_classification].classification ==
		    TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE)
		return false;
	/*
	 * An inherited proof is only valid for two source leaves that describe
	 * the same decoded operation and encoding.  A matching condition alone
	 * is insufficient, because it would allow unrelated leaves to borrow a
	 * canonical proof merely by sharing a feature condition.
	 */
	if (strcmp(source[source_index].operation_id,
		   source[canonical_source].operation_id) ||
	    source[source_index].mask != source[canonical_source].mask ||
	    source[source_index].pattern != source[canonical_source].pattern ||
	    strcmp(source[source_index].condition_tcnd_hex,
		   source[canonical_source].condition_tcnd_hex))
		return false;
	return !empty(row->proof_id) &&
	       !strcmp(row->proof_id,
		       classification[canonical_classification].proof_id);
}

static bool classification_row_well_formed(
	const struct tcti_target_completion_classification_row *row)
{
	return row && !empty(row->name);
}

/*
 * This validates immutable source-to-test provenance. It does not observe a
 * KUnit or kselftest result and therefore must not be described as proof.
 */
static bool source_proof_binding_resolves(
	const struct tcti_target_completion_source_row *source,
	const struct tcti_target_completion_classification_row *row,
	const struct tcti_target_proof_registry_entry *registry,
	size_t registry_count)
{
	const struct tcti_target_proof_reference reference = {
		.id = row->proof_id,
		.leaf_name = source->name,
		.mnemonic = source->mnemonic,
		.operation_id = source->operation_id,
		.encoding_mask = source->mask,
		.encoding_pattern = source->pattern,
		.condition_tcnd_hex = source->condition_tcnd_hex,
		.classification = row->classification,
	};

	return tcti_target_proof_registry_lookup(
		       registry, registry_count, &reference) ==
	       TCTI_TARGET_PROOF_REGISTRY_OK;
}

static void validate_registry_bindings(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_classification_row *classification,
	size_t classification_count,
	const struct tcti_target_proof_registry_entry *registry,
	size_t registry_count, struct tcti_target_completion_result *result)
{
	size_t entry_index;

	for (entry_index = 0; entry_index < registry_count; entry_index++) {
		const struct tcti_target_proof_registry_entry *entry =
			&registry[entry_index];
		size_t binding_index;

		for (binding_index = 0;
		     binding_index < entry->binding_count;
		     binding_index++) {
			const struct tcti_target_proof_binding *binding =
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
				    TCTI_TARGET_COMPLETION_UNCLASSIFIED)
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
					TCTI_TARGET_COMPLETION_ERROR_STALE_PROOF_BINDING);
			}
		}
	}
}

static void validate_unproved_obligations(
	const struct tcti_target_proof_registry_entry *registry,
	size_t registry_count, struct tcti_target_completion_result *result)
{
	size_t entry_index;

	for (entry_index = 0; entry_index < registry_count; entry_index++) {
		const struct tcti_target_proof_registry_entry *entry =
			&registry[entry_index];
		size_t binding_index;

		if (!entry->unproved_obligations)
			continue;
		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++) {
			result->unproved_obligation_bindings++;
			record_error(
				result,
				TCTI_TARGET_COMPLETION_ERROR_UNPROVED_OBLIGATIONS);
		}
	}
}

int tcti_target_completion_validate(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_classification_row *classification,
	size_t classification_count,
	const struct tcti_target_proof_registry_entry *registry,
	size_t registry_count,
	struct tcti_target_completion_result *result)
{
	enum tcti_target_proof_registry_error registry_error;
	bool registry_valid = true;
	size_t index;

	if (!result)
		return -1;
	memset(result, 0, sizeof(*result));
	result->source_rows = source_count;
	result->classification_rows = classification_count;

	if (!source || source_count != TCTI_TARGET_COMPLETION_SOURCE_ROWS) {
		record_error(result, TCTI_TARGET_COMPLETION_ERROR_SOURCE_COUNT);
		if (!source)
			return -1;
	}
	if (!classification ||
	    classification_count != TCTI_TARGET_COMPLETION_SOURCE_ROWS) {
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_CLASSIFICATION_COUNT);
		if (!classification)
			return -1;
	}
	if (tcti_target_proof_registry_validate(registry, registry_count,
						&registry_error)) {
		registry_valid = false;
		result->invalid_registry_entries++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_PROOF_REGISTRY);
	}

	for (index = 0; index < source_count; index++) {
		size_t previous;

		if (!source_row_matches_canonical(&source[index], index)) {
			result->invalid_source_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_SOURCE);
		}
		for (previous = 0; previous < index; previous++)
			if (source_row_well_formed(&source[index], index) &&
			    source_row_well_formed(&source[previous], previous) &&
			    !strcmp(source[index].name,
				    source[previous].name)) {
				result->invalid_source_rows++;
				record_error(
					result,
					TCTI_TARGET_COMPLETION_ERROR_SOURCE);
				break;
			}
		if (source_row_well_formed(&source[index], index) &&
		    find_classification(classification,
					classification_count,
					source[index].name) ==
		    classification_count) {
			result->absent_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_ABSENT);
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
		const struct tcti_target_completion_classification_row *row =
			&classification[index];
		size_t source_index =
			find_source(source, source_count, row->name);
		size_t previous;
		bool relationship_valid;

		if (!classification_row_well_formed(row) ||
		    source_index == source_count) {
			result->stale_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_STALE);
			continue;
		}
		for (previous = 0; previous < index; previous++)
			if (classification_row_well_formed(row) &&
			    classification_row_well_formed(&classification[previous]) &&
			    !strcmp(row->name,
				    classification[previous].name)) {
				result->stale_rows++;
				record_error(result,
					     TCTI_TARGET_COMPLETION_ERROR_STALE);
				break;
			}
		if (row->classification ==
		    TCTI_TARGET_COMPLETION_UNCLASSIFIED) {
			result->unclassified_rows++;
			record_error(
				result,
				TCTI_TARGET_COMPLETION_ERROR_UNCLASSIFIED);
			continue;
		}
		if (row->classification >
		    TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE) {
			result->invalid_relationship_rows++;
			record_error(
				result,
				TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP);
			continue;
		}
		result->classified_rows++;
		switch (row->classification) {
		case TCTI_TARGET_COMPLETION_REQUIRED_EL0:
			result->required_el0_rows++;
			break;
		case TCTI_TARGET_COMPLETION_NON_EL0:
			result->non_el0_rows++;
			break;
		case TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED:
			result->undefined_or_unallocated_rows++;
			break;
		case TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE:
			result->alias_or_duplicate_rows++;
			break;
		case TCTI_TARGET_COMPLETION_UNCLASSIFIED:
			break;
		}

		relationship_valid =
			row->classification ==
				TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE ?
			valid_alias_relationship(
				source, source_count, classification,
				classification_count, index) :
			valid_non_alias_relationship(row);
		if (!relationship_valid) {
			result->invalid_relationship_rows++;
			record_error(
				result,
				TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP);
		}

		if (empty(row->evidence) || empty(row->proof_id)) {
			result->source_unbound_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
			continue;
		}
		if (row->classification ==
		    TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE) {
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
				if (relationship_valid &&
				    !alias_binding_valid) {
					result->invalid_relationship_rows++;
					record_error(
						result,
						TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP);
				}
				result->source_unbound_rows++;
				record_error(
					result,
					TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
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
				     TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING);
		}
	}
	return result->errors ? -1 : 0;
}

int tcti_target_completion_validate_source_provenance(
	const struct tcti_target_completion_source_provenance *provenance,
	struct tcti_target_completion_result *result)
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
		TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH ||
	    provenance->leaf_count != TCTI_TARGET_COMPLETION_SOURCE_ROWS) {
		result->invalid_source_provenance++;
		record_error(result,
			     TCTI_TARGET_COMPLETION_ERROR_SOURCE_PROVENANCE);
		return -1;
	}
	return 0;
}

int tcti_target_completion_audit(struct tcti_target_completion_result *result)
{
	const struct tcti_target_proof_registry_entry *registry;
	int status;
	size_t registry_count;

	if (!result)
		return -1;

	registry = tcti_target_proof_registry_entries(&registry_count);
	status = tcti_target_completion_validate(
		source_rows, sizeof(source_rows) / sizeof(source_rows[0]),
		classification_rows,
		sizeof(classification_rows) / sizeof(classification_rows[0]),
		registry, registry_count, result);
	if (tcti_target_completion_validate_source_provenance(
		    &source_provenance, result))
		status = -1;
	if (tcti_target_completion_validate_asl_availability(
		    source_rows, sizeof(source_rows) / sizeof(source_rows[0]),
		    &asl_provenance, asl_rows,
		    sizeof(asl_rows) / sizeof(asl_rows[0]), result))
		status = -1;
	if (tcti_target_completion_validate_system_accessors(
		    source_rows, sizeof(source_rows) / sizeof(source_rows[0]),
		    &system_accessor_provenance, system_accessor_rows,
		    sizeof(system_accessor_rows) / sizeof(system_accessor_rows[0]),
		    result))
		status = -1;
	if (tcti_target_completion_validate_feature_field_domains(
		    tcti_feature_artifact_canonical(),
		    tcti_feature_field_domain_binding_artifact_canonical(), result))
		status = -1;
	if (tcti_target_completion_validate_runtime_capability_cohorts(
		    tcti_runtime_capability_cohort_artifact_canonical(), result))
		status = -1;
	validate_source_feature_domain(source_rows,
			       sizeof(source_rows) / sizeof(source_rows[0]), result);
	if (result->invalid_feature_artifact ||
	    result->invalid_source_condition_rows ||
	    result->unresolved_feature_applicability_rows)
		status = -1;
	return status;
}

const struct tcti_target_completion_source_provenance *
tcti_target_completion_source_provenance(void)
{
	return &source_provenance;
}

const struct tcti_target_completion_asl_provenance *
tcti_target_completion_asl_provenance(void)
{
	return &asl_provenance;
}

const struct tcti_target_completion_asl_row *
tcti_target_completion_asl_availability(size_t *count)
{
	if (count)
		*count = sizeof(asl_rows) / sizeof(asl_rows[0]);
	return asl_rows;
}

const struct tcti_target_completion_system_accessor_provenance *
tcti_target_completion_system_accessor_provenance(void)
{
	return &system_accessor_provenance;
}

const struct tcti_target_completion_system_accessor_row *
tcti_target_completion_system_accessors(size_t *count)
{
	if (count)
		*count =
			sizeof(system_accessor_rows) /
			sizeof(system_accessor_rows[0]);
	return system_accessor_rows;
}

const struct tcti_target_completion_source_row *
tcti_target_completion_source(size_t *count)
{
	if (count)
		*count = sizeof(source_rows) / sizeof(source_rows[0]);
	return source_rows;
}

const struct tcti_target_completion_classification_row *
tcti_target_completion_classification(size_t *count)
{
	if (count)
		*count =
			sizeof(classification_rows) /
			sizeof(classification_rows[0]);
	return classification_rows;
}
