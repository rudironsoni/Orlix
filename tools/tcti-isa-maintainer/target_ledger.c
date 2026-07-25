/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_ledger.h"
#include "target_condition_serialization.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define stringify_1(value) #value
#define stringify(value) stringify_1(value)
#define UNCLASSIFIED TCTI_TARGET_LEDGER_UNCLASSIFIED
#define REQUIRED TCTI_TARGET_LEDGER_REQUIRED_EL0
#define REQUIRED_EL0 TCTI_TARGET_LEDGER_REQUIRED_EL0
#define NON_EL0 TCTI_TARGET_LEDGER_NON_EL0
#define ARCHITECTURALLY_UNDEFINED TCTI_TARGET_LEDGER_ARCH_UNDEFINED_OR_UNALLOCATED
#define ARCH_UNDEFINED_OR_UNALLOCATED TCTI_TARGET_LEDGER_ARCH_UNDEFINED_OR_UNALLOCATED
#define ALIAS_OR_DUPLICATE TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, pattern, condition, ...) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition },
static const struct tcti_target_ledger_source_row source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, canonical, evidence, proof) \
	{ stringify(name), classification, relation, canonical, evidence, proof, \
	  NULL, NULL, NULL },
static const struct tcti_target_ledger_classification_row classification_rows[] = {
#include "../isa/target_classification.def"
};
#undef TCTI_A64_TARGET_CLASSIFICATION

static bool empty(const char *text)
{
	return !text || !text[0];
}

static void error(struct tcti_target_ledger_result *result,
		  enum tcti_target_ledger_error code, size_t index)
{
	if (result->first_error == TCTI_TARGET_LEDGER_OK) {
		result->first_error = code;
		result->first_error_index = index;
	}
	result->errors++;
}

static size_t find_source(const struct tcti_target_ledger_source_row *source,
			  size_t count, const char *name)
{
	size_t i;

	for (i = 0; i < count; i++)
		if (!strcmp(source[i].name, name))
			return i;
	return count;
}

static size_t find_classification(const struct tcti_target_ledger_classification_row *classification,
				  size_t count, const char *name)
{
	size_t i;

	for (i = 0; i < count; i++)
		if (!strcmp(classification[i].name, name))
			return i;
	return count;
}

static bool hex_matches(const char *hex, const uint8_t *bytes, size_t length)
{
	static const char digits[] = "0123456789abcdef";
	size_t i;

	if (strlen(hex) != length * 2)
		return false;
	for (i = 0; i < length; i++)
		if (hex[i * 2] != digits[bytes[i] >> 4] ||
		    hex[i * 2 + 1] != digits[bytes[i] & 0xf])
			return false;
	return true;
}

static void validate_proof_reference(
	const struct tcti_target_ledger_source_row *source_row,
	const struct tcti_target_ledger_classification_row *classification_row,
	const struct tcti_target_proof_registry_entry *proof_registry,
	size_t proof_registry_count, size_t error_index,
	struct tcti_target_ledger_result *result)
{
	const struct tcti_target_proof_reference reference = {
		.id = classification_row->proof,
		.leaf_name = source_row->name,
		.mnemonic = source_row->mnemonic,
		.operation_id = source_row->operation_id,
		.encoding_mask = source_row->mask,
		.encoding_pattern = source_row->pattern,
		.condition_tcnd_hex = source_row->condition_tcnd_hex,
		.classification = classification_row->classification,
	};
	enum tcti_target_proof_registry_error proof_error;
	enum tcti_target_ledger_error ledger_error;

	proof_error = tcti_target_proof_registry_lookup(proof_registry,
							 proof_registry_count,
							 &reference);
	if (proof_error == TCTI_TARGET_PROOF_REGISTRY_OK)
		return;
	result->proof_gaps++;
	switch (proof_error) {
	case TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_PROOF:
		ledger_error = TCTI_TARGET_LEDGER_UNKNOWN_PROOF;
		break;
	case TCTI_TARGET_PROOF_REGISTRY_CLASSIFICATION_MISMATCH:
		ledger_error = TCTI_TARGET_LEDGER_PROOF_CLASSIFICATION_MISMATCH;
		break;
	case TCTI_TARGET_PROOF_REGISTRY_FAMILY_MISMATCH:
		ledger_error = TCTI_TARGET_LEDGER_PROOF_FAMILY_MISMATCH;
		break;
	case TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS:
		ledger_error = TCTI_TARGET_LEDGER_PROOF_INSUFFICIENT_OBLIGATIONS;
		break;
	case TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_FAMILY_REQUIREMENTS:
		ledger_error =
			TCTI_TARGET_LEDGER_UNKNOWN_PROOF_FAMILY_REQUIREMENTS;
		break;
	case TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH:
		ledger_error = TCTI_TARGET_LEDGER_PROOF_BINDING_MISMATCH;
		break;
	default:
		ledger_error = TCTI_TARGET_LEDGER_INVALID_PROOF_REGISTRY;
		break;
	}
	error(result, ledger_error, error_index);
}

static size_t operation_proof_count(
	const struct tcti_target_proof_registry_entry *proof_registry,
	size_t proof_registry_count, const char *operation_id)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < proof_registry_count; index++)
		if (proof_registry[index].classification_mask ==
			    TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0 &&
		    !strcmp(proof_registry[index].operation_id, operation_id))
			count++;
	return count;
}

static void validate_registry_bindings(
	const struct tcti_target_ledger_source_row *source,
	size_t source_count,
	const struct tcti_target_ledger_classification_row *classification,
	size_t classification_count,
	const struct tcti_target_proof_registry_entry *proof_registry,
	size_t proof_registry_count, struct tcti_target_ledger_result *result)
{
	size_t entry_index;

	for (entry_index = 0; entry_index < proof_registry_count; entry_index++) {
		const struct tcti_target_proof_registry_entry *entry =
			&proof_registry[entry_index];
		size_t binding_index;

		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++) {
			const struct tcti_target_proof_binding *binding =
				&entry->bindings[binding_index];
			size_t source_index = find_source(source, source_count,
							  binding->leaf_name);
			size_t classification_index;

			if (source_index == source_count) {
				error(result,
				      TCTI_TARGET_LEDGER_UNKNOWN_PROOF_BINDING_LEAF,
				      entry_index);
				continue;
			}
			if (strcmp(source[source_index].mnemonic, binding->mnemonic) ||
			    source[source_index].mask != binding->encoding_mask ||
			    source[source_index].pattern != binding->encoding_pattern ||
			    strcmp(source[source_index].condition_tcnd_hex,
				   binding->condition_tcnd_hex))
				error(result, TCTI_TARGET_LEDGER_STALE_PROOF_BINDING,
				      entry_index);
			if (strcmp(source[source_index].operation_id,
				   entry->operation_id))
				error(result,
				      TCTI_TARGET_LEDGER_PROOF_BINDING_OPERATION_MISMATCH,
				      entry_index);
			classification_index = find_classification(
				classification, classification_count,
				binding->leaf_name);
			if (classification_index == classification_count ||
			    classification[classification_index].classification >
				    TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE ||
			    entry->classification_mask !=
				    (1U << classification[classification_index].classification))
				error(result,
				      TCTI_TARGET_LEDGER_PROOF_BINDING_CLASSIFICATION_MISMATCH,
				      entry_index);
		}
	}
}

int tcti_target_ledger_validate(const struct tcti_target_ledger_source_row *source,
				size_t source_count,
				const struct tcti_target_ledger_classification_row *classification,
				size_t classification_count,
				size_t required_count,
				const struct tcti_target_proof_registry_entry *proof_registry,
				size_t proof_registry_count,
				struct tcti_target_ledger_result *result)
{
	size_t i, j;
	enum tcti_target_proof_registry_error proof_error;

	if (!result)
		return -1;
	memset(result, 0, sizeof(*result));
	result->source_rows = source_count;
	if (!source || source_count != required_count ||
	    source_count > TCTI_A64_TARGET_LEAF_COUNT) {
		error(result, TCTI_TARGET_LEDGER_BAD_COUNT, source_count);
		return -1;
	}
	if (!classification || classification_count != required_count ||
	    classification_count > TCTI_A64_TARGET_LEAF_COUNT) {
		error(result, TCTI_TARGET_LEDGER_BAD_COUNT,
		      classification_count);
		return -1;
	}
	if (tcti_target_proof_registry_validate(proof_registry,
						 proof_registry_count,
						 &proof_error)) {
		error(result, TCTI_TARGET_LEDGER_INVALID_PROOF_REGISTRY, 0);
		return -1;
	}
	validate_registry_bindings(source, source_count, classification,
				   classification_count, proof_registry,
				   proof_registry_count, result);
	for (i = 0; i < source_count; i++) {
		if (source[i].ordinal != i)
			error(result, TCTI_TARGET_LEDGER_BAD_ORDINAL, i);
		for (j = 0; j < i; j++)
			if (!strcmp(source[i].name, source[j].name))
				error(result, TCTI_TARGET_LEDGER_DUPLICATE_SOURCE_NAME, i);
	}
	for (i = 0; i < classification_count; i++) {
		const struct tcti_target_ledger_classification_row *row =
			&classification[i];

		if (empty(row->name) ||
		    find_source(source, source_count, row->name) == source_count)
			error(result, TCTI_TARGET_LEDGER_STALE_CLASSIFICATION, i);
		for (j = 0; j < i; j++)
			if (!strcmp(row->name, classification[j].name))
				error(result, TCTI_TARGET_LEDGER_DUPLICATE_CLASSIFICATION_NAME, i);
	}
	for (i = 0; i < source_count; i++) {
		const struct tcti_target_ledger_classification_row *row;

		j = find_classification(classification, classification_count,
					source[i].name);
		if (j == classification_count) {
			error(result, TCTI_TARGET_LEDGER_MISSING_CLASSIFICATION, i);
			continue;
		}
		row = &classification[j];
		if (row->classification == TCTI_TARGET_LEDGER_UNCLASSIFIED) {
			result->unclassified_rows++;
			error(result, TCTI_TARGET_LEDGER_ERROR_UNCLASSIFIED, i);
			continue;
		}
		if (row->classification > TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE) {
			error(result, TCTI_TARGET_LEDGER_INVALID_CLASSIFICATION, i);
			continue;
		}
		result->classified_rows++;
		if (row->classification != TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE) {
			uint32_t ignored_requirements;

			if (tcti_target_proof_operation_requirements(
				    source[i].operation_id, row->classification,
				    &ignored_requirements))
				error(result,
				      TCTI_TARGET_LEDGER_UNKNOWN_OPERATION_REQUIREMENTS,
				      i);
		}
		if (row->classification == TCTI_TARGET_LEDGER_REQUIRED_EL0) {
			bool first_operation = true;
			size_t previous_source;

			for (previous_source = 0; previous_source < i;
			     previous_source++) {
				size_t previous_classification = find_classification(
					classification, classification_count,
					source[previous_source].name);

				if (previous_classification < classification_count &&
				    classification[previous_classification].classification ==
					    TCTI_TARGET_LEDGER_REQUIRED_EL0 &&
				    !strcmp(source[previous_source].operation_id,
					    source[i].operation_id)) {
					first_operation = false;
					break;
				}
			}
			if (first_operation &&
			    operation_proof_count(proof_registry,
					  proof_registry_count,
					  source[i].operation_id) != 1)
				error(result,
				      TCTI_TARGET_LEDGER_REQUIRED_OPERATION_PROOF_COUNT,
				      i);
		}
		if (empty(row->evidence) || empty(row->proof)) {
			result->proof_gaps++;
			error(result, empty(row->evidence) ? TCTI_TARGET_LEDGER_MISSING_EVIDENCE :
			      TCTI_TARGET_LEDGER_MISSING_PROOF, i);
		} else if (row->classification !=
			   TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE)
			validate_proof_reference(&source[i], row, proof_registry,
						 proof_registry_count, i, result);
		if (row->classification == TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE) {
			char expected_relation[512];
			const char *relation_name;
			int relation_length;
			size_t target;

			relation_name = row->relation == TCTI_A64_TARGET_RELATION_ALIAS ?
				"alias" : "duplicate";
			relation_length = snprintf(expected_relation,
						  sizeof(expected_relation),
						  "alias=%s;canonical=%s;relation=%s",
						  row->name ? row->name : "",
						  row->canonical_name ?
							  row->canonical_name : "",
						  relation_name);
			if (empty(row->relation_evidence) ||
			    empty(row->relation_source) ||
			    empty(row->relation_source_sha256))
				error(result,
				      TCTI_TARGET_LEDGER_MISSING_RELATION_EVIDENCE, i);
			else if (relation_length < 0 ||
				 relation_length >= (int)sizeof(expected_relation) ||
				 strcmp(row->relation_evidence, expected_relation) ||
				 tcti_target_proof_source_evidence_validate(
					 row->relation_source,
					 row->relation_source_sha256,
					 row->relation_evidence))
				error(result,
				      TCTI_TARGET_LEDGER_INVALID_RELATION_EVIDENCE, i);
			if (row->relation != TCTI_A64_TARGET_RELATION_ALIAS &&
			    row->relation != TCTI_A64_TARGET_RELATION_DUPLICATE) {
				error(result, TCTI_TARGET_LEDGER_INVALID_RELATION, i);
				continue;
			}
			if (empty(row->canonical_name)) {
				error(result, TCTI_TARGET_LEDGER_MISSING_CANONICAL, i);
				continue;
			}
			target = find_classification(classification,
						     classification_count,
						     row->canonical_name);
			if (target == classification_count)
				error(result, TCTI_TARGET_LEDGER_UNKNOWN_CANONICAL, i);
			else if (target == j)
				error(result, TCTI_TARGET_LEDGER_SELF_CANONICAL, i);
			else if (classification[target].classification == TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE)
				error(result, TCTI_TARGET_LEDGER_NONCANONICAL_TARGET, i);
			else {
				size_t canonical_source =
					find_source(source, source_count,
						    row->canonical_name);

				if (canonical_source == source_count) {
					error(result,
					      TCTI_TARGET_LEDGER_UNKNOWN_CANONICAL, i);
					continue;
				}
				if (strcmp(source[i].condition_tcnd_hex,
					   source[canonical_source].condition_tcnd_hex))
					error(result,
					      TCTI_TARGET_LEDGER_CONDITION_MISMATCH, i);
				if (strcmp(row->proof, classification[target].proof))
					error(result,
					      TCTI_TARGET_LEDGER_ALIAS_PROOF_MISMATCH, i);
				else if (!empty(row->proof))
					validate_proof_reference(
						&source[canonical_source],
						&classification[target],
						proof_registry,
						proof_registry_count, i, result);
			}
		} else if (row->relation != TCTI_A64_TARGET_RELATION_NONE || !empty(row->canonical_name)) {
			error(result, TCTI_TARGET_LEDGER_UNEXPECTED_CANONICAL, i);
		}
	}
	return result->errors ? -1 : 0;
}

int tcti_target_ledger_verify_import(const struct tcti_target_ledger_source_row *source,
				     size_t count,
				     const struct tcti_target_inventory *inventory,
				     struct tcti_target_ledger_result *result)
{
	size_t i;

	if (!result || !source || !inventory)
		return -1;
	if (inventory->leaf_count != count) {
		error(result, TCTI_TARGET_LEDGER_BAD_COUNT, inventory->leaf_count);
		return -1;
	}
	for (i = 0; i < count; i++) {
		struct tcti_target_condition_bytes bytes = { 0 };
		const struct tcti_target_leaf *leaf = &inventory->leaves[i];
		const struct tcti_target_ledger_source_row *row = &source[i];

		if (strcmp(row->name, leaf->name) || strcmp(row->mnemonic, leaf->mnemonic) ||
		    strcmp(row->operation_id, leaf->operation_id) || row->mask != leaf->encoding_mask ||
		    row->pattern != leaf->encoding_pattern) {
			error(result, TCTI_TARGET_LEDGER_STALE_SOURCE_FACT, i);
			continue;
		}
		if (tcti_target_condition_serialize(inventory, leaf->condition, &bytes, NULL) ||
		    !hex_matches(row->condition_tcnd_hex, bytes.data, bytes.length))
			error(result, TCTI_TARGET_LEDGER_STALE_SOURCE_CONDITION, i);
		tcti_target_condition_bytes_destroy(&bytes);
	}
	return result->errors ? -1 : 0;
}

const struct tcti_target_ledger_source_row *tcti_target_ledger_source(size_t *count)
{
	if (count)
		*count = sizeof(source_rows) / sizeof(source_rows[0]);
	return source_rows;
}

const struct tcti_target_ledger_classification_row *tcti_target_ledger_classification(size_t *count)
{
	if (count)
		*count = sizeof(classification_rows) / sizeof(classification_rows[0]);
	return classification_rows;
}
