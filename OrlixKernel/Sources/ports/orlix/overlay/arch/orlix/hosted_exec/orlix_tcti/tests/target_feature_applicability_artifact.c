/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_applicability_artifact.h"
#include "target_feature_artifact.h"
#include "target_feature_domain.h"
#include "target_feature_field_domain_binding_artifact.h"
#include "target_instruction_artifact.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <stdbool.h>
#include <string.h>
#endif

#define APPLICABILITY_ARCHITECTURE "vFAPA2-A"
#define APPLICABILITY_RELEASE "2026-06_rel"
#define APPLICABILITY_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"
#define APPLICABILITY_FEATURES_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"
#define APPLICABILITY_REGISTERS_SHA256 \
	"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"
#define APPLICABILITY_RECONCILIATION_IDENTITY \
	"ceb8f8c561a5ecdce1a32bda12c7f33fc1b0433bbf035301f2590b6000ccbfe4"
#define U32_NONE ((orlix_tcti_feature_applicability_u32)-1)

#define IGNORE_SOURCE(...)
#define IGNORE_COUNTS(...)
#define IGNORE_ROW(...)
#define IGNORE_SYMBOL(...)
#define IGNORE_COMMON(...)
#define IGNORE_OPERAND(...)

static const struct orlix_tcti_target_feature_applicability_provenance
canonical_provenance = {
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(architecture, release, \
	instructions, features, registers, reconciliation, rows, parameters) \
	architecture, release, instructions, features, registers, reconciliation, \
	rows, parameters
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS IGNORE_COUNTS
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW IGNORE_ROW
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL IGNORE_SYMBOL
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES IGNORE_COMMON
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE IGNORE_OPERAND
#include "../isa/target_feature_applicability.def"
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE
};

#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE IGNORE_SOURCE
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW IGNORE_ROW
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL IGNORE_SYMBOL
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES IGNORE_COMMON
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE IGNORE_OPERAND
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(common, rows, operands, bytes) \
	enum { CANONICAL_COMMON_SYMBOLS = common, CANONICAL_COMMON_ROWS = rows, \
		CANONICAL_OPERANDS = operands, CANONICAL_COMMON_BYTES = bytes };
#include "../isa/target_feature_applicability.def"
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE

#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE IGNORE_SOURCE
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS IGNORE_COUNTS
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL IGNORE_SYMBOL
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES IGNORE_COMMON
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE IGNORE_OPERAND
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(ordinal, name, mnemonic, operation, \
	condition, condition_length, status, condition_hex, witness, witness_count, \
	first_operand, operand_count, formula) \
	{ ordinal, name, mnemonic, operation, condition, condition_length, status, \
	  condition_hex, (const signed char *)(witness), witness_count, \
	  sizeof(witness) - 1U, first_operand, operand_count, formula },
static const struct orlix_tcti_target_feature_applicability_row canonical_rows[] = {
#include "../isa/target_feature_applicability.def"
};
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE

#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE IGNORE_SOURCE
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS IGNORE_COUNTS
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW IGNORE_ROW
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES IGNORE_COMMON
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE IGNORE_OPERAND
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL(index, kind, numeric, node, group, name, width) \
	[index] = { kind, numeric, node, group, U32_NONE, name, width, 0, 0 },
static const struct orlix_tcti_target_feature_applicability_certificate_value
canonical_common_symbols[] = {
#include "../isa/target_feature_applicability.def"
};
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE

#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE IGNORE_SOURCE
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS IGNORE_COUNTS
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW IGNORE_ROW
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL IGNORE_SYMBOL
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE IGNORE_OPERAND
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES(row, ...) \
	[row] = { { __VA_ARGS__ } },
static const struct orlix_tcti_target_feature_applicability_common_values
	canonical_common_values[] = {
#include "../isa/target_feature_applicability.def"
};
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE

#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE IGNORE_SOURCE
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS IGNORE_COUNTS
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW IGNORE_ROW
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL IGNORE_SYMBOL
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES IGNORE_COMMON
#define ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE(index, leaf, name, width, low, high) \
	[index] = { ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_TARGET_OPERAND, \
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSIGNED, U32_NONE, U32_NONE, \
		leaf, name, width, low, high },
static const struct orlix_tcti_target_feature_applicability_certificate_value
canonical_operand_values[] = {
#include "../isa/target_feature_applicability.def"
};
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE

static const struct orlix_tcti_target_feature_applicability_artifact
canonical_artifact = {
	.provenance = &canonical_provenance,
	.rows = canonical_rows,
	.row_count = sizeof(canonical_rows) / sizeof(canonical_rows[0]),
	.common_symbols = canonical_common_symbols,
	.common_symbol_count = sizeof(canonical_common_symbols) /
		sizeof(canonical_common_symbols[0]),
	.common_values = canonical_common_values,
	.common_value_row_count = sizeof(canonical_common_values) /
		sizeof(canonical_common_values[0]),
	.operand_values = canonical_operand_values,
	.operand_value_count = sizeof(canonical_operand_values) /
		sizeof(canonical_operand_values[0]),
	.common_value_bytes_per_row = CANONICAL_COMMON_BYTES,
};

_Static_assert(sizeof(canonical_rows) / sizeof(canonical_rows[0]) ==
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_ROWS, "exact row count");
_Static_assert(sizeof(canonical_common_symbols) / sizeof(canonical_common_symbols[0]) ==
	CANONICAL_COMMON_SYMBOLS, "exact symbol count");
_Static_assert(sizeof(canonical_common_values) / sizeof(canonical_common_values[0]) ==
	CANONICAL_COMMON_ROWS, "exact common-value row count");
_Static_assert(sizeof(canonical_operand_values) / sizeof(canonical_operand_values[0]) ==
	CANONICAL_OPERANDS, "exact operand count");
_Static_assert(CANONICAL_OPERANDS ==
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OPERAND_VALUES,
	"exact unique leaf/operand certificate census");
_Static_assert(ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_HEX_LENGTH %
	32U == 0U, "common-value chunks preserve value boundaries");
_Static_assert(CANONICAL_COMMON_BYTES * 2U <=
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_COUNT *
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_HEX_LENGTH,
	"common-value chunks cover every canonical value");

static bool empty(const char *text) { return !text || !text[0]; }

static int nibble(char digit)
{
	if (digit >= '0' && digit <= '9') return digit - '0';
	if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
	return -1;
}

static bool sha256_valid(const char *value)
{
	size_t index;
	if (!value || strlen(value) != 64U) return false;
	for (index = 0; index < 64U; index++) if (nibble(value[index]) < 0) return false;
	return true;
}

static int fail(struct orlix_tcti_target_feature_applicability_validation_result *result,
	enum orlix_tcti_target_feature_applicability_validation_error error, size_t row)
{
	if (result) { result->error = error; result->row = row; }
	return -1;
}

static bool numeric_valid(
	const struct orlix_tcti_target_feature_applicability_certificate_value *value)
{
	orlix_tcti_feature_applicability_u64 mask;
	if (!value || !value->width || value->width > 128U ||
	    value->numeric_kind > ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SIGNED)
		return false;
	if (value->width <= 64U)
		return !value->high && (value->width == 64U || !(value->low >> value->width));
	mask = value->width == 128U ? ~(orlix_tcti_feature_applicability_u64)0 :
		((orlix_tcti_feature_applicability_u64)1U << (value->width - 64U)) - 1U;
	return !(value->high & ~mask);
}

static int decode_u64(const char *hex, orlix_tcti_feature_applicability_u64 *value)
{
	size_t index;
	orlix_tcti_feature_applicability_u64 parsed = 0;
	for (index = 0; index < 16U; index++) {
		int part = nibble(hex[index]);
		if (part < 0) return -1;
		parsed = (parsed << 4) | (unsigned int)part;
	}
	*value = parsed;
	return 0;
}

static int common_value(
	const struct orlix_tcti_target_feature_applicability_artifact *artifact,
	size_t row, size_t symbol,
	struct orlix_tcti_target_feature_applicability_certificate_value *value)
{
	const char *cursor;
	size_t hex_offset;
	size_t chunk;
	size_t chunk_offset;
	if (row >= artifact->common_value_row_count || symbol >= artifact->common_symbol_count)
		return -1;
	hex_offset = symbol * 32U;
	chunk = hex_offset /
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_HEX_LENGTH;
	chunk_offset = hex_offset %
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_HEX_LENGTH;
	if (chunk >= ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_COUNT ||
	    !artifact->common_values[row].chunks[chunk])
		return -1;
	cursor = artifact->common_values[row].chunks[chunk] + chunk_offset;
	*value = artifact->common_symbols[symbol];
	return decode_u64(cursor, &value->low) || decode_u64(cursor + 16U, &value->high) ||
		!numeric_valid(value) ? -1 : 0;
}

static bool condition_matches(const char *hex,
	const struct orlix_tcti_target_instruction_artifact *artifact,
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf)
{
	size_t index;
	if (!hex || strlen(hex) != (size_t)leaf->condition_length * 2U ||
	    leaf->condition_offset > artifact->condition_pool_size ||
	    leaf->condition_length > artifact->condition_pool_size - leaf->condition_offset)
		return false;
	for (index = 0; index < leaf->condition_length; index++) {
		int high = nibble(hex[index * 2U]);
		int low = nibble(hex[index * 2U + 1U]);
		if (high < 0 || low < 0 || (unsigned char)((high << 4) | low) !=
		    artifact->condition_pool[leaf->condition_offset + index]) return false;
	}
	return true;
}

static bool identifier_is_parameter(
	const struct orlix_tcti_feature_artifact *artifact,
	const struct orlix_tcti_feature_artifact_node *node)
{
	size_t index;

	if (!node->text)
		return false;
	for (index = 0; index < artifact->counts.parameter_count; index++)
		if (artifact->parameters[index].name &&
		    !strcmp(artifact->parameters[index].name, node->text))
			return true;
	return false;
}

static bool dot_atom_equal(const struct orlix_tcti_feature_artifact *artifact,
	orlix_tcti_feature_artifact_u32 left,
	orlix_tcti_feature_artifact_u32 right)
{
	const struct orlix_tcti_feature_artifact_node *left_node;
	const struct orlix_tcti_feature_artifact_node *right_node;
	orlix_tcti_feature_artifact_u32 item;

	if (left >= artifact->counts.node_count || right >= artifact->counts.node_count)
		return false;
	left_node = &artifact->nodes[left];
	right_node = &artifact->nodes[right];
	if (left_node->kind != ORLIX_TCTI_FEATURE_ARTIFACT_DOT_ATOM ||
	    right_node->kind != ORLIX_TCTI_FEATURE_ARTIFACT_DOT_ATOM ||
	    left_node->child_count != right_node->child_count)
		return false;
	for (item = 0; item < left_node->child_count; item++) {
		orlix_tcti_feature_artifact_u32 left_child =
			artifact->children[left_node->first_child + item];
		orlix_tcti_feature_artifact_u32 right_child =
			artifact->children[right_node->first_child + item];

		if (left_child >= artifact->counts.node_count ||
		    right_child >= artifact->counts.node_count ||
		    artifact->nodes[left_child].kind !=
			ORLIX_TCTI_FEATURE_ARTIFACT_IDENTIFIER ||
		    artifact->nodes[right_child].kind !=
			ORLIX_TCTI_FEATURE_ARTIFACT_IDENTIFIER ||
		    !artifact->nodes[left_child].text ||
		    !artifact->nodes[right_child].text ||
		    strcmp(artifact->nodes[left_child].text,
			artifact->nodes[right_child].text))
			return false;
	}
	return true;
}

int orlix_tcti_target_feature_applicability_artifact_validate(
	const struct orlix_tcti_target_feature_applicability_artifact *artifact,
	struct orlix_tcti_target_feature_applicability_validation_result *result)
{
	const struct orlix_tcti_target_instruction_artifact *instructions =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_feature_artifact *features =
		orlix_tcti_feature_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result instruction_result;
	size_t expected_operand = 0;
	size_t configuration_count = 0, field_count = 0, boolean_count = 0;
	size_t index;
	if (result) *result = (struct orlix_tcti_target_feature_applicability_validation_result){0};
	if (!artifact || !artifact->provenance || !artifact->rows || !artifact->common_symbols ||
	 !artifact->common_values || (!artifact->operand_values && artifact->operand_value_count))
		return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ARGUMENT, 0);
	if (orlix_tcti_target_instruction_artifact_validate(instructions, &instruction_result))
		return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CONDITION_BINDING,
			instruction_result.leaf_index);
	if (!sha256_valid(artifact->provenance->instructions_sha256) ||
	    !sha256_valid(artifact->provenance->features_sha256) ||
	    !sha256_valid(artifact->provenance->registers_sha256) ||
	    !sha256_valid(artifact->provenance->reconciliation_identity) ||
	    strcmp(artifact->provenance->architecture, APPLICABILITY_ARCHITECTURE) ||
	    strcmp(artifact->provenance->release, APPLICABILITY_RELEASE) ||
	    strcmp(artifact->provenance->instructions_sha256, APPLICABILITY_INSTRUCTIONS_SHA256) ||
	    strcmp(artifact->provenance->features_sha256, APPLICABILITY_FEATURES_SHA256) ||
	    strcmp(artifact->provenance->registers_sha256, APPLICABILITY_REGISTERS_SHA256) ||
	    strcmp(artifact->provenance->reconciliation_identity,
		APPLICABILITY_RECONCILIATION_IDENTITY))
		return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_PROVENANCE, 0);
	if (artifact->row_count != ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_ROWS ||
	    artifact->common_value_row_count != artifact->row_count ||
	    artifact->operand_value_count !=
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OPERAND_VALUES ||
	    artifact->common_symbol_count !=
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_VALUES ||
	    artifact->common_symbol_count >
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_MAX_COMMON_VALUES ||
	    artifact->common_value_bytes_per_row != artifact->common_symbol_count * 16U ||
	    artifact->provenance->row_count != artifact->row_count ||
	    artifact->provenance->parameter_count !=
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PARAMETERS)
		return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_COUNT, 0);
	for (index = 0; index < artifact->common_symbol_count; index++) {
		const struct orlix_tcti_target_feature_applicability_certificate_value *symbol =
			&artifact->common_symbols[index];
		size_t prior;
		if (!symbol->width || symbol->width > 128U || symbol->numeric_kind >
		    ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SIGNED ||
		    symbol->leaf_index != U32_NONE)
			return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
		if (symbol->kind == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONFIGURATION) {
			const struct orlix_tcti_feature_artifact_node *node;

			if (symbol->feature_node_index >= features->counts.node_count)
				return fail(result,
					ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE,
					0);
			node = &features->nodes[symbol->feature_node_index];
			if (node->kind != ORLIX_TCTI_FEATURE_ARTIFACT_DOT_ATOM ||
			    symbol->identity_group_index != U32_NONE || !empty(symbol->name))
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
			configuration_count++;
		} else if (symbol->kind ==
			   ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_BOOLEAN_IDENTIFIER) {
			const struct orlix_tcti_feature_artifact_node *node;

			if (symbol->feature_node_index >= features->counts.node_count)
				return fail(result,
					ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE,
					0);
			node = &features->nodes[symbol->feature_node_index];
			if (node->kind != ORLIX_TCTI_FEATURE_ARTIFACT_IDENTIFIER ||
			    symbol->numeric_kind != ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSIGNED ||
			    symbol->width != 1U || symbol->identity_group_index != U32_NONE ||
			    empty(symbol->name) || strcmp(symbol->name, node->text) ||
			    identifier_is_parameter(features, node))
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
			boolean_count++;
		} else if (symbol->kind == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_FIELD) {
			if (symbol->feature_node_index != U32_NONE ||
			    symbol->identity_group_index >=
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT ||
			    !empty(symbol->name))
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
			field_count++;
		} else return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
		for (prior = 0; prior < index; prior++)
			if (artifact->common_symbols[prior].kind == symbol->kind &&
			    ((symbol->kind == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONFIGURATION &&
			      dot_atom_equal(features,
				artifact->common_symbols[prior].feature_node_index,
				symbol->feature_node_index)) ||
			     (symbol->kind ==
				      ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_BOOLEAN_IDENTIFIER &&
			      !strcmp(artifact->common_symbols[prior].name, symbol->name)) ||
			     (symbol->kind == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_FIELD &&
			      artifact->common_symbols[prior].identity_group_index == symbol->identity_group_index)))
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
	}
	if (configuration_count !=
			ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONFIGURATION_VALUES ||
	    field_count != ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_FIELD_VALUES ||
	    boolean_count != ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_BOOLEAN_VALUES)
		return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_COUNT, 0);
	for (index = 0; index < artifact->row_count; index++) {
		const struct orlix_tcti_target_feature_applicability_row *row = &artifact->rows[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
			&instructions->leaves[index];
		const char *name = (const char *)instructions->string_pool + leaf->name_offset;
		const char *mnemonic = (const char *)instructions->string_pool + leaf->mnemonic_offset;
		const char *operation = (const char *)instructions->string_pool + leaf->operation_offset;
		size_t witness;
		size_t operand;
		size_t chunk;
		size_t remaining_hex = artifact->common_symbol_count * 32U;
		for (chunk = 0;
		     chunk < ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_COUNT;
		     chunk++) {
			size_t expected = remaining_hex >
				ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_HEX_LENGTH ?
				ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_HEX_LENGTH :
				remaining_hex;
			const char *text = artifact->common_values[index].chunks[chunk];

			if ((expected && (!text || strlen(text) != expected)) ||
			    (!expected && text))
				return fail(result,
					ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE,
					index);
			remaining_hex -= expected;
		}
		if (remaining_hex)
			return fail(result,
				ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE,
				index);
		for (witness = 0; witness < artifact->common_symbol_count; witness++) {
			struct orlix_tcti_target_feature_applicability_certificate_value value;
			if (common_value(artifact, index, witness, &value))
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, index);
		}
		if (row->ordinal != index || row->status != ORLIX_TCTI_TARGET_FEATURE_APPLICABLE ||
		    row->condition_index != leaf->condition_offset ||
		    row->condition_length != leaf->condition_length ||
		    !condition_matches(row->condition_tcnd_hex, instructions, leaf) ||
		    strcmp(row->name, name) || strcmp(row->mnemonic, mnemonic) ||
		    strcmp(row->operation_id, operation))
			return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ROW, index);
		if (!row->witness || row->witness_count !=
		    ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PARAMETERS ||
		    row->witness_storage_length != row->witness_count)
			return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_WITNESS, index);
		for (witness = 0; witness < row->witness_count; witness++)
			if (row->witness[witness] != 1 && row->witness[witness] != -1)
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_WITNESS, index);
		if (!row->formula_identity || row->first_operand_value != expected_operand ||
		    row->operand_value_count >
			ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_MAX_OPERAND_VALUES ||
		    row->first_operand_value > artifact->operand_value_count ||
		    row->operand_value_count > artifact->operand_value_count - row->first_operand_value)
			return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, index);
		for (operand = 0; operand < row->operand_value_count; operand++) {
			const struct orlix_tcti_target_feature_applicability_certificate_value *value =
				&artifact->operand_values[row->first_operand_value + operand];
			size_t prior;
			if (!numeric_valid(value) || value->kind !=
			    ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_TARGET_OPERAND ||
			    value->numeric_kind != ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSIGNED ||
			    value->feature_node_index != U32_NONE || value->identity_group_index != U32_NONE ||
			    value->leaf_index != index || empty(value->name) || value->width > 32U)
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, index);
			for (prior = 0; prior < operand; prior++)
				if (!strcmp(artifact->operand_values[row->first_operand_value + prior].name,
					value->name))
					return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, index);
		}
		expected_operand += row->operand_value_count;
	}
	if (expected_operand != artifact->operand_value_count)
		return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
	if (result) { result->error = ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_VALID;
		result->applicable_count = artifact->row_count; result->impossible_count = 0; }
	return 0;
}

struct semantic_context {
	const struct orlix_tcti_feature_artifact *features;
	const struct orlix_tcti_feature_field_domain_binding_artifact *fields;
	const struct orlix_tcti_target_feature_applicability_artifact *artifact;
	const struct orlix_tcti_target_feature_applicability_row *row;
	size_t row_index;
	const size_t *parameter_order;
	const size_t *binding_order;
	const size_t *field_common_index;
	unsigned char *common_seen;
	orlix_tcti_feature_applicability_u64 operands_seen;
	char operand_value[35];
};

static void domain_value(
	const struct orlix_tcti_target_feature_applicability_certificate_value *source,
	struct orlix_tcti_feature_domain_value *value)
{
	*value = (struct orlix_tcti_feature_domain_value){0};
	value->kind = source->numeric_kind == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SIGNED ?
		ORLIX_TCTI_FEATURE_DOMAIN_VALUE_SIGNED : ORLIX_TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED;
	value->integer.low = source->low; value->integer.high = source->high;
	value->integer.width = (orlix_tcti_feature_artifact_u8)source->width;
}

static int semantic_feature(void *opaque, const char *name,
	struct orlix_tcti_feature_domain_value *value)
{
	struct semantic_context *context = opaque;
	struct orlix_tcti_target_feature_applicability_certificate_value decoded;
	size_t found = SIZE_MAX;
	size_t index;
	size_t low = 0, high = context->features->counts.parameter_count;
	while (low < high) {
		size_t middle = low + (high - low) / 2U;
		size_t parameter = context->parameter_order[middle];
		int order = strcmp(name, context->features->parameters[parameter].name);
		if (order < 0) high = middle; else if (order > 0) low = middle + 1U;
		else { value->kind = ORLIX_TCTI_FEATURE_DOMAIN_VALUE_BOOL;
			value->boolean = context->row->witness[parameter] > 0; return 0; }
	}
	for (index = 0; index < context->artifact->common_symbol_count; index++) {
		const struct orlix_tcti_target_feature_applicability_certificate_value
			*symbol = &context->artifact->common_symbols[index];
		const struct orlix_tcti_feature_artifact_node *node;

		if (symbol->kind !=
				ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_BOOLEAN_IDENTIFIER ||
		    symbol->feature_node_index >= context->features->counts.node_count)
			continue;
		node = &context->features->nodes[symbol->feature_node_index];
		if (node->kind != ORLIX_TCTI_FEATURE_ARTIFACT_IDENTIFIER ||
		    !node->text || strcmp(node->text, name) || strcmp(symbol->name, name))
			continue;
		if (found != SIZE_MAX)
			return -1;
		found = index;
	}
	if (found == SIZE_MAX ||
	    common_value(context->artifact, context->row_index, found, &decoded) ||
	    decoded.width != 1U || decoded.high || decoded.low > 1U)
		return -1;
	*value = (struct orlix_tcti_feature_domain_value){0};
	value->kind = ORLIX_TCTI_FEATURE_DOMAIN_VALUE_BOOL;
	value->boolean = decoded.low != 0U;
	context->common_seen[found] = 1U;
	return 0;
}

static int semantic_configuration(void *opaque, orlix_tcti_feature_artifact_u32 node,
	enum orlix_tcti_feature_domain_value_kind kind, orlix_tcti_feature_artifact_u32 width,
	struct orlix_tcti_feature_domain_value *value)
{
	struct semantic_context *context = opaque;
	size_t index, found = SIZE_MAX;
	struct orlix_tcti_target_feature_applicability_certificate_value decoded;
	for (index = 0; index < context->artifact->common_symbol_count; index++)
		if (context->artifact->common_symbols[index].kind ==
				ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONFIGURATION &&
		    dot_atom_equal(context->features,
			context->artifact->common_symbols[index].feature_node_index, node)) {
			if (found != SIZE_MAX) return -1; found = index;
		}
	if (found == SIZE_MAX || common_value(context->artifact, context->row_index, found, &decoded) ||
	    width != decoded.width || kind != (decoded.numeric_kind ==
	    ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SIGNED ?
	    ORLIX_TCTI_FEATURE_DOMAIN_VALUE_SIGNED : ORLIX_TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED)) return -1;
	domain_value(&decoded, value); context->common_seen[found] = 1U; return 0;
}

static int binding_compare(const struct orlix_tcti_feature_field_domain_binding *binding,
	const char *state, const char *reg, const char *selector)
{
	int order = strcmp(state, binding->field_state);
	if (!order) order = strcmp(reg, binding->field_register_name);
	if (!order) order = strcmp(selector, binding->field_selector);
	return order;
}

static int semantic_field(void *opaque, const char *state, const char *reg,
	const char *selector, struct orlix_tcti_feature_domain_value *value)
{
	struct semantic_context *context = opaque;
	size_t low = 0, high = context->fields->occurrence_count, match;
	orlix_tcti_feature_artifact_u32 identity;
	struct orlix_tcti_target_feature_applicability_certificate_value decoded;
	while (low < high) {
		size_t middle = low + (high - low) / 2U;
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&context->fields->bindings[context->binding_order[middle]];
		int order = binding_compare(binding, state, reg, selector);
		if (order < 0) high = middle; else if (order > 0) low = middle + 1U;
		else { match = middle; goto found; }
	}
	return -1;
found:
	identity = context->fields->bindings[context->binding_order[match]].identity_group_index;
	while (match && !binding_compare(&context->fields->bindings[
		context->binding_order[match - 1U]], state, reg, selector)) match--;
	for (; match < context->fields->occurrence_count; match++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&context->fields->bindings[context->binding_order[match]];
		if (binding_compare(binding, state, reg, selector)) break;
		if (binding->identity_group_index != identity) return -1;
	}
	if (identity >= context->fields->identity_group_count ||
	    context->field_common_index[identity] == SIZE_MAX ||
	    common_value(context->artifact, context->row_index,
		context->field_common_index[identity], &decoded)) return -1;
	domain_value(&decoded, value);
	context->common_seen[context->field_common_index[identity]] = 1U;
	return 0;
}

static int hex_byte(const char *hex, size_t byte)
{
	int high = nibble(hex[byte * 2U]), low = nibble(hex[byte * 2U + 1U]);
	return high < 0 || low < 0 ? -1 : (high << 4) | low;
}

static int token_compare(const char *hex, size_t offset, size_t length, const char *name)
{
	size_t index, name_length = strlen(name);
	for (index = 0; index < length && index < name_length; index++) {
		int byte = hex_byte(hex, offset + index);
		if (byte < 0 || byte != (unsigned char)name[index])
			return byte < 0 ? -2 : byte - (unsigned char)name[index];
	}
	return length < name_length ? -1 : length > name_length ? 1 : 0;
}

static int semantic_tcnd_feature(void *opaque, const char *hex, size_t offset,
	size_t length, orlix_tcti_feature_artifact_u8 *enabled)
{
	struct semantic_context *context = opaque;
	size_t low = 0, high = context->features->counts.parameter_count;
	while (low < high) {
		size_t middle = low + (high - low) / 2U;
		size_t parameter = context->parameter_order[middle];
		int order = token_compare(hex, offset, length,
			context->features->parameters[parameter].name);
		if (order < 0) high = middle; else if (order > 0) low = middle + 1U;
		else { *enabled = context->row->witness[parameter] > 0; return 0; }
	}
	return -1;
}

static int semantic_tcnd_operand(void *opaque, const char *hex, size_t offset,
	size_t length, const char **value, size_t *value_length)
{
	struct semantic_context *context = opaque;
	size_t index, found = SIZE_MAX;
	for (index = 0; index < context->row->operand_value_count; index++) {
		const struct orlix_tcti_target_feature_applicability_certificate_value *candidate =
			&context->artifact->operand_values[context->row->first_operand_value + index];
		if (!token_compare(hex, offset, length, candidate->name)) {
			if (found != SIZE_MAX) return -1; found = index;
		}
	}
	if (found == SIZE_MAX) return -1;
	{
		const struct orlix_tcti_target_feature_applicability_certificate_value *source =
			&context->artifact->operand_values[context->row->first_operand_value + found];
		size_t bit;
		context->operand_value[0] = '\'';
		for (bit = 0; bit < source->width; bit++)
			context->operand_value[bit + 1U] = (char)('0' +
				((source->low >> (source->width - bit - 1U)) & 1U));
		context->operand_value[source->width + 1U] = '\'';
		context->operand_value[source->width + 2U] = '\0';
		*value = context->operand_value; *value_length = source->width + 2U;
		context->operands_seen |= (orlix_tcti_feature_applicability_u64)1U << found;
	}
	return 0;
}

static int parameter_less(const struct orlix_tcti_feature_artifact *artifact,
	size_t left, size_t right)
{ return strcmp(artifact->parameters[left].name, artifact->parameters[right].name) < 0; }

static int binding_less(const struct orlix_tcti_feature_field_domain_binding_artifact *artifact,
	size_t left, size_t right)
{
	const struct orlix_tcti_feature_field_domain_binding *a = &artifact->bindings[left];
	const struct orlix_tcti_feature_field_domain_binding *b = &artifact->bindings[right];
	int order = strcmp(a->field_state, b->field_state);
	if (!order) order = strcmp(a->field_register_name, b->field_register_name);
	if (!order) order = strcmp(a->field_selector, b->field_selector);
	return order < 0;
}

int orlix_tcti_target_feature_applicability_artifact_validate_semantics(
	const struct orlix_tcti_target_feature_applicability_artifact *artifact,
	struct orlix_tcti_target_feature_applicability_semantic_scratch *scratch,
	struct orlix_tcti_target_feature_applicability_validation_result *result)
{
	const struct orlix_tcti_feature_artifact *features = orlix_tcti_feature_artifact_canonical();
	const struct orlix_tcti_feature_field_domain_binding_artifact *fields =
		orlix_tcti_feature_field_domain_binding_artifact_canonical();
	size_t index;
	if (orlix_tcti_target_feature_applicability_artifact_validate(artifact, result)) return -1;
	if (!scratch || !scratch->active_feature_nodes ||
	    scratch->active_feature_node_count < features->counts.node_count ||
	    !scratch->common_values_seen || scratch->common_values_seen_count < artifact->common_symbol_count ||
	    !scratch->parameter_order || scratch->parameter_order_count < features->counts.parameter_count ||
	    !scratch->binding_order || scratch->binding_order_count < fields->occurrence_count ||
	    !scratch->field_common_index || scratch->field_common_index_count < fields->identity_group_count)
		return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ARGUMENT, 0);
	for (index = 0; index < features->counts.parameter_count; index++) {
		size_t position = index; scratch->parameter_order[index] = index;
		while (position && parameter_less(features, scratch->parameter_order[position],
			scratch->parameter_order[position - 1U])) {
			size_t swap = scratch->parameter_order[position];
			scratch->parameter_order[position] = scratch->parameter_order[position - 1U];
			scratch->parameter_order[--position] = swap;
		}
	}
	for (index = 0; index < fields->occurrence_count; index++) {
		size_t position = index; scratch->binding_order[index] = index;
		while (position && binding_less(fields, scratch->binding_order[position],
			scratch->binding_order[position - 1U])) {
			size_t swap = scratch->binding_order[position];
			scratch->binding_order[position] = scratch->binding_order[position - 1U];
			scratch->binding_order[--position] = swap;
		}
	}
	for (index = 0; index < fields->identity_group_count; index++)
		scratch->field_common_index[index] = SIZE_MAX;
	for (index = 0; index < artifact->common_symbol_count; index++)
		if (artifact->common_symbols[index].kind == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_FIELD) {
			size_t group = artifact->common_symbols[index].identity_group_index;
			if (scratch->field_common_index[group] != SIZE_MAX)
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE, 0);
			scratch->field_common_index[group] = index;
		}
	for (index = 0; index < artifact->row_count; index++) {
		const struct orlix_tcti_target_feature_applicability_row *row = &artifact->rows[index];
		struct semantic_context context = { features, fields, artifact, row, index,
			scratch->parameter_order, scratch->binding_order, scratch->field_common_index,
			scratch->common_values_seen, 0, {0} };
		struct orlix_tcti_feature_domain_environment environment = {
			&context, semantic_feature, semantic_configuration, semantic_field };
		struct orlix_tcti_feature_domain_scratch domain_scratch = {
			scratch->active_feature_nodes, scratch->active_feature_node_count,
			features->counts.node_count };
		struct orlix_tcti_feature_domain_tcnd_environment tcnd = {
			&context, semantic_tcnd_feature, semantic_tcnd_operand };
		struct orlix_tcti_feature_domain_diagnostic diagnostic = {0};
		struct orlix_tcti_feature_domain_tcnd_diagnostic tcnd_diagnostic = {0};
		orlix_tcti_feature_artifact_u8 satisfied = 0;
		size_t common;
		orlix_tcti_feature_applicability_u64 expected = row->operand_value_count == 64U ?
			~(orlix_tcti_feature_applicability_u64)0 :
			((orlix_tcti_feature_applicability_u64)1U << row->operand_value_count) - 1U;
		memset(scratch->active_feature_nodes, 0, features->counts.node_count);
		memset(scratch->common_values_seen, 0, artifact->common_symbol_count);
		if (orlix_tcti_feature_domain_evaluate_constraints(features, &environment,
			&domain_scratch, &satisfied, &diagnostic) || !satisfied)
			return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SEMANTIC_MISMATCH, index);
		for (common = 0; common < artifact->common_symbol_count; common++)
			if (!scratch->common_values_seen[common])
				return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SEMANTIC_MISMATCH, index);
		satisfied = 0;
		if (orlix_tcti_feature_domain_evaluate_tcnd(features, row->condition_tcnd_hex,
			&tcnd, &satisfied, &tcnd_diagnostic) || !satisfied || context.operands_seen != expected)
			return fail(result, ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SEMANTIC_MISMATCH, index);
	}
	return 0;
}

const struct orlix_tcti_target_feature_applicability_artifact *
orlix_tcti_target_feature_applicability_artifact(void)
{
	return &canonical_artifact;
}
