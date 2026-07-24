/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_completion_audit.h"

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
					schema_value, sha_value, count_value) \
	arch_value, build_value, release_value, schema_value, sha_value, count_value
#define TCTI_A64_SOURCE_MANIFEST_ROW(...)
#include "../isa/source_manifest.def"
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, \
				      pattern, condition) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition },
static const struct tcti_target_completion_source_row source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

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

static bool empty(const char *text)
{
	return !text || !text[0];
}

static bool source_row_well_formed(
	const struct tcti_target_completion_source_row *row, size_t ordinal)
{
	return row && row->ordinal == ordinal && !empty(row->name) &&
		!empty(row->mnemonic) && !empty(row->operation_id) &&
		!empty(row->condition_tcnd_hex);
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

static bool proof_resolves(
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

		if (!source_row_well_formed(&source[index], index)) {
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
			result->unproved_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_UNPROVED);
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
			    !proof_resolves(
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
				result->unproved_rows++;
				record_error(
					result,
					TCTI_TARGET_COMPLETION_ERROR_UNPROVED);
			} else {
				result->proved_rows++;
			}
		} else if (registry_valid &&
			   proof_resolves(&source[source_index], row,
					  registry, registry_count)) {
			result->proved_rows++;
		} else {
			result->unproved_rows++;
			record_error(result,
				     TCTI_TARGET_COMPLETION_ERROR_UNPROVED);
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
	    empty(provenance->schema) || empty(provenance->source_sha256) ||
	    strcmp(provenance->architecture, "vFATAp1-A") ||
	    strcmp(provenance->build, "818") ||
	    strcmp(provenance->release, "2026-06_rel") ||
	    strcmp(provenance->schema, "2.9.5") ||
	    strcmp(provenance->source_sha256,
		   "a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe") ||
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
	return status;
}

const struct tcti_target_completion_source_provenance *
tcti_target_completion_source_provenance(void)
{
	return &source_provenance;
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
