/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_applicability_generator.h"
#include "target_condition_serialization.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ORLIX_TCTI_AARCHMRS_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"
#define ORLIX_TCTI_AARCHMRS_FEATURES_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"
#define ORLIX_TCTI_AARCHMRS_REGISTERS_SHA256 \
	"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"
#define ORLIX_TCTI_AARCHMRS_RECONCILIATION_IDENTITY \
	"ceb8f8c561a5ecdce1a32bda12c7f33fc1b0433bbf035301f2590b6000ccbfe4"

static int emit_c_string(FILE *output, const char *text)
{
	const unsigned char *cursor = (const unsigned char *)text;

	if (fputc('"', output) == EOF)
		return -1;
	while (*cursor) {
		if (*cursor == '"' || *cursor == '\\') {
			if (fputc('\\', output) == EOF || fputc(*cursor, output) == EOF)
				return -1;
		} else if (*cursor >= 0x20U && *cursor <= 0x7eU) {
			if (fputc(*cursor, output) == EOF)
				return -1;
		} else if (fprintf(output, "\\%03o", (unsigned int)*cursor) < 0) {
			return -1;
		}
		cursor++;
	}
	return fputc('"', output) == EOF ? -1 : 0;
}

static int emit_witness(FILE *output, const signed char *witness, size_t count)
{
	size_t index;

	if (fputc('"', output) == EOF)
		return -1;
	for (index = 0; index < count; index++)
		if (fprintf(output, "\\%03o", (unsigned int)(uint8_t)witness[index]) < 0)
			return -1;
	return fputc('"', output) == EOF ? -1 : 0;
}

static int emit_condition_hex(FILE *output,
	const struct orlix_tcti_target_inventory *inventory, uint32_t condition)
{
	static const char digits[] = "0123456789abcdef";
	struct orlix_tcti_target_condition_bytes bytes = { 0 };
	size_t index;
	int result = -1;

	if (orlix_tcti_target_condition_serialize(inventory, condition, &bytes, NULL) ||
	    fputc('"', output) == EOF)
		goto out;
	for (index = 0; index < bytes.length; index++)
		if (fputc(digits[bytes.data[index] >> 4], output) == EOF ||
		    fputc(digits[bytes.data[index] & 0xfU], output) == EOF)
			goto out;
	if (fputc('"', output) != EOF)
		result = 0;
out:
	orlix_tcti_target_condition_bytes_destroy(&bytes);
	return result;
}

static enum orlix_tcti_target_feature_applicability_error validate(
	const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_target_feature_sat_audit *audit)
{
	size_t index;

	if (!model || !inventory || !audit)
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ARGUMENT;
	if (inventory->leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT)
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PIN_MISMATCH;
	if (!inventory->expression_count || !inventory->expressions)
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONDITION_INVALID;
	if (audit->leaf_count != inventory->leaf_count ||
	    audit->parameter_count != model->parameter_count || !audit->leaves)
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_AUDIT_MISMATCH;
	if (model->parameter_count && !audit->witnesses)
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_UNAVAILABLE;
	for (index = 0; index < inventory->leaf_count; index++) {
		const signed char *witness;
		size_t parameter;

		if (inventory->leaves[index].condition >= inventory->expression_count)
			return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONDITION_INVALID;
		if (audit->leaves[index] == ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE)
			return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSAT_UNCERTIFIED;
		if (audit->leaves[index] != ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE)
			return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_AUDIT_MISMATCH;
		if (!model->parameter_count)
			continue;
		witness = orlix_tcti_target_feature_sat_witness(audit, index);
		if (!witness)
			return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_UNAVAILABLE;
		for (parameter = 0; parameter < model->parameter_count; parameter++)
			if (witness[parameter] != -1 && witness[parameter] != 1)
				return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_INVALID;
	}
	return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK;
}

static int copy_staged(FILE *stage, FILE *output)
{
	unsigned char buffer[4096];
	size_t count;

	if (fflush(stage) || fseek(stage, 0, SEEK_SET))
		return -1;
	while ((count = fread(buffer, 1, sizeof(buffer), stage)) != 0)
		if (fwrite(buffer, 1, count, output) != count)
			return -1;
	return ferror(stage) || ferror(output) ? -1 : 0;
}

static int build_condition_map(const struct orlix_tcti_target_inventory *inventory,
	uint32_t **offsets_out, uint32_t **lengths_out)
{
	uint32_t *offsets;
	uint32_t *lengths;
	size_t total = 0;
	size_t index;

	offsets = calloc(inventory->expression_count, sizeof(*offsets));
	lengths = calloc(inventory->expression_count, sizeof(*lengths));
	if ((inventory->expression_count && !offsets) ||
	    (inventory->expression_count && !lengths)) {
		free(lengths);
		free(offsets);
		return -1;
	}
	for (index = 0; index < inventory->expression_count; index++) {
		struct orlix_tcti_target_condition_bytes bytes = { 0 };

		if (orlix_tcti_target_condition_serialize(inventory, (uint32_t)index,
			&bytes, NULL) || total > UINT32_MAX || bytes.length > UINT32_MAX ||
		    bytes.length > UINT32_MAX - total) {
			orlix_tcti_target_condition_bytes_destroy(&bytes);
			free(lengths);
			free(offsets);
			return -1;
		}
		offsets[index] = (uint32_t)total;
		lengths[index] = (uint32_t)bytes.length;
		total += bytes.length;
		orlix_tcti_target_condition_bytes_destroy(&bytes);
	}
	*offsets_out = offsets;
	*lengths_out = lengths;
	return 0;
}

static int certificate_value_valid(
	const struct orlix_tcti_target_feature_sat_value *value)
{
	uint64_t high_mask;

	if (!value || !value->width || value->width > 128U ||
	    value->numeric_kind > ORLIX_TCTI_TARGET_FEATURE_SAT_SIGNED)
		return 0;
	if (value->width <= 64U)
		return !value->high &&
		(value->width == 64U || !(value->low >> value->width));
	high_mask = value->width == 128U ? UINT64_MAX :
		(UINT64_C(1) << (value->width - 64U)) - 1U;
	return !(value->high & ~high_mask);
}

static int common_symbol_valid(
	const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_feature_sat_value *value)
{
	const struct orlix_tcti_feature_node *node;

	if (!certificate_value_valid(value))
		return 0;
	if (value->kind == ORLIX_TCTI_TARGET_FEATURE_SAT_CONFIGURATION) {
		if (value->feature_node_index >= model->node_count)
			return 0;
		node = &model->nodes[value->feature_node_index];
		return node->kind == ORLIX_TCTI_FEATURE_DOT_ATOM &&
			value->identity_group_index == UINT32_MAX &&
			value->leaf_index == UINT32_MAX &&
			(!value->name || !value->name[0]);
	}
	if (value->kind == ORLIX_TCTI_TARGET_FEATURE_SAT_BOOLEAN_IDENTIFIER) {
		if (value->feature_node_index >= model->node_count)
			return 0;
		node = &model->nodes[value->feature_node_index];
		return node->kind == ORLIX_TCTI_FEATURE_IDENTIFIER && node->text &&
			value->numeric_kind == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSIGNED &&
			value->width == 1U && value->identity_group_index == UINT32_MAX &&
			value->leaf_index == UINT32_MAX && value->name &&
			!strcmp(value->name, node->text);
	}
	if (value->kind == ORLIX_TCTI_TARGET_FEATURE_SAT_FIELD)
		return value->feature_node_index == UINT32_MAX &&
			value->identity_group_index != UINT32_MAX &&
			value->leaf_index == UINT32_MAX && (!value->name || !value->name[0]);
	return 0;
}

static int common_symbol_matches(
	const struct orlix_tcti_target_feature_sat_value *left,
	const struct orlix_tcti_target_feature_sat_value *right)
{
	return left->kind == right->kind &&
		left->numeric_kind == right->numeric_kind &&
		left->feature_node_index == right->feature_node_index &&
		left->identity_group_index == right->identity_group_index &&
		left->width == right->width &&
		((!left->name && !right->name) ||
		 (left->name && right->name && !strcmp(left->name, right->name)));
}

static int operand_value_valid(
	const struct orlix_tcti_target_feature_sat_value *value, size_t leaf_index)
{
	return certificate_value_valid(value) &&
		value->kind == ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND &&
		value->numeric_kind == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSIGNED &&
		value->feature_node_index == UINT32_MAX &&
		value->identity_group_index == UINT32_MAX &&
		value->leaf_index == leaf_index && value->name && value->name[0] &&
		value->width <= 32U;
}

static int certificate_layout(
	const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_target_feature_sat_audit *audit,
	size_t *common_count_out, size_t *value_bytes_out, size_t *operand_count_out)
{
	const struct orlix_tcti_target_feature_sat_value *canonical = NULL;
	size_t common_count = 0;
	size_t value_bytes;
	size_t operand_count = 0;
	size_t expected_first = 0;
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < inventory->leaf_count; leaf_index++) {
		const struct orlix_tcti_target_feature_sat_certificate *certificate =
			orlix_tcti_target_feature_sat_certificate(audit, leaf_index);
		const struct orlix_tcti_target_feature_sat_value *values;
		size_t count = 0;
		size_t index;

		if (!certificate || certificate->condition_index !=
			inventory->leaves[leaf_index].condition || !certificate->formula_identity)
			return -1;
		if (certificate->first_value != expected_first)
			return -1;
		values = orlix_tcti_target_feature_sat_certificate_values(audit,
			leaf_index, &count);
		if (!values || !count)
			return -1;
		expected_first += count;
		if (!canonical) {
			while (common_count < count &&
			       values[common_count].kind !=
				ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND) {
				if (!common_symbol_valid(model, &values[common_count]))
					return -1;
				for (index = 0; index < common_count; index++) {
					if (values[index].kind != values[common_count].kind)
						continue;
					if (values[index].kind ==
					    ORLIX_TCTI_TARGET_FEATURE_SAT_CONFIGURATION &&
					    values[index].feature_node_index ==
						values[common_count].feature_node_index)
						return -1;
					if (values[index].kind == ORLIX_TCTI_TARGET_FEATURE_SAT_FIELD &&
					    values[index].identity_group_index ==
						values[common_count].identity_group_index)
						return -1;
				}
				common_count++;
			}
			if (!common_count)
				return -1;
			canonical = values;
		}
		if (count < common_count)
			return -1;
		for (index = 0; index < common_count; index++)
			if (!common_symbol_valid(model, &values[index]) ||
			    !common_symbol_matches(&canonical[index], &values[index]))
				return -1;
		for (index = common_count; index < count; index++) {
			size_t other;

			if (!operand_value_valid(&values[index], leaf_index))
				return -1;
			for (other = common_count; other < index; other++)
				if (!strcmp(values[index].name, values[other].name))
					return -1;
			operand_count++;
		}
	}
	if (expected_first != audit->certificate_value_count)
		return -1;
	if (common_count > SIZE_MAX / 16U)
		return -1;
	value_bytes = common_count * 16U;
	*common_count_out = common_count;
	*value_bytes_out = value_bytes;
	*operand_count_out = operand_count;
	return 0;
}

static int emit_common_values(FILE *output, size_t leaf_index,
	const struct orlix_tcti_target_feature_sat_value *values,
	size_t common_count)
{
	size_t index;

	if (fprintf(output,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES(%zuU,\n",
		leaf_index) < 0)
		return -1;
	for (index = 0; index < common_count; index++) {
		if (!(index % 16U) && fputs("\t\"", output) == EOF)
			return -1;
		if (fprintf(output, "%016" PRIx64 "%016" PRIx64,
			values[index].low, values[index].high) < 0)
			return -1;
		if ((index % 16U == 15U || index + 1U == common_count) &&
		    fputs(index + 1U == common_count ? "\"\n" : "\",\n",
			  output) == EOF)
			return -1;
	}
	return fputs(")\n", output) == EOF ? -1 : 0;
}

const char *orlix_tcti_target_feature_applicability_error_name(
	enum orlix_tcti_target_feature_applicability_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK: return "success";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PIN_MISMATCH: return "pinned inventory mismatch";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_AUDIT_MISMATCH: return "SAT audit mismatch";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_UNAVAILABLE: return "SAT witness unavailable";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_INVALID: return "SAT witness invalid";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CERTIFICATE_INVALID: return "SAT certificate invalid";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSAT_UNCERTIFIED: return "UNSAT result lacks a checked certificate";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONDITION_INVALID: return "instruction condition invalid";
	case ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_IO: return "I/O failure";
	}
	return "unknown failure";
}

enum orlix_tcti_target_feature_applicability_error
orlix_tcti_target_feature_applicability_emit(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_target_feature_sat_audit *audit, FILE *output)
{
	enum orlix_tcti_target_feature_applicability_error error;
	FILE *stage;
	uint32_t *condition_offsets = NULL;
	uint32_t *condition_lengths = NULL;
	size_t common_value_count;
	size_t certificate_value_bytes;
	size_t operand_value_count;
	size_t first_operand = 0;
	size_t index;

	if (!output)
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ARGUMENT;
	error = validate(model, inventory, audit);
	if (error != ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK)
		return error;
	if (build_condition_map(inventory, &condition_offsets, &condition_lengths))
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONDITION_INVALID;
	if (certificate_layout(model, inventory, audit, &common_value_count,
		&certificate_value_bytes, &operand_value_count)) {
		free(condition_lengths);
		free(condition_offsets);
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CERTIFICATE_INVALID;
	}
	stage = tmpfile();
	if (!stage) {
		free(condition_lengths);
		free(condition_offsets);
		return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_IO;
	}
	if (fprintf(stage,
		"/* SPDX-License-Identifier: BSD-3-Clause */\n"
		"/* Generated by target_feature_applicability_generator.c. Do not edit. */\n"
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(\"vFAPA2-A\", \"2026-06_rel\", \"%s\", \"%s\", \"%s\", \"%s\", %uU, %zuU)\n",
		ORLIX_TCTI_AARCHMRS_INSTRUCTIONS_SHA256, ORLIX_TCTI_AARCHMRS_FEATURES_SHA256,
		ORLIX_TCTI_AARCHMRS_REGISTERS_SHA256,
		ORLIX_TCTI_AARCHMRS_RECONCILIATION_IDENTITY,
		ORLIX_TCTI_A64_TARGET_LEAF_COUNT, model->parameter_count) < 0)
		goto io;
	if (fprintf(stage,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(%zuU, %zuU, %zuU, %zuU)\n",
		common_value_count, inventory->leaf_count, operand_value_count,
		certificate_value_bytes) < 0)
		goto io;
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory->leaves[index];
		const struct orlix_tcti_target_feature_sat_certificate *certificate =
			orlix_tcti_target_feature_sat_certificate(audit, index);
		const signed char *witness = NULL;
		unsigned int status = audit->leaves[index] ==
			ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE ? 1U : 0U;

		if (status && model->parameter_count)
			witness = orlix_tcti_target_feature_sat_witness(audit, index);
		if (fprintf(stage, "ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(%zuU, ", index) < 0 ||
		    emit_c_string(stage, leaf->name ? leaf->name : "") ||
		    fputs(", ", stage) == EOF ||
		    emit_c_string(stage, leaf->mnemonic ? leaf->mnemonic : "") ||
		    fputs(", ", stage) == EOF ||
		    emit_c_string(stage, leaf->operation_id ? leaf->operation_id : "") ||
		    fprintf(stage, ", %" PRIu32 "U, %" PRIu32 "U, %uU, ",
			condition_offsets[leaf->condition],
			condition_lengths[leaf->condition], status) < 0 ||
		    emit_condition_hex(stage, inventory, leaf->condition) ||
		    fputs(", ", stage) == EOF ||
		    emit_witness(stage, witness, status ? model->parameter_count : 0U) ||
		    fprintf(stage, ", %zuU, %zuU, %zuU, 0x%016" PRIx64 "ULL)\n",
			status ? model->parameter_count : 0U, first_operand,
			certificate->value_count - common_value_count,
			certificate->formula_identity) < 0)
			goto io;
		first_operand += certificate->value_count - common_value_count;
	}
	for (index = 0; index < common_value_count; index++) {
		const struct orlix_tcti_target_feature_sat_value *value =
			&audit->certificate_values[index];

		if (fprintf(stage,
			"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL(%zuU, %uU, %uU, %" PRIu32 "U, %" PRIu32 "U, ",
			index, (unsigned int)value->kind,
			(unsigned int)value->numeric_kind,
			value->feature_node_index,
			value->identity_group_index) < 0 ||
		    emit_c_string(stage, value->name ? value->name : "") ||
		    fprintf(stage, ", %uU)\n", (unsigned int)value->width) < 0)
			goto io;
	}
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct orlix_tcti_target_feature_sat_value *values;
		size_t value_count = 0;

		values = orlix_tcti_target_feature_sat_certificate_values(
			audit, index, &value_count);
		if (!values || value_count < common_value_count ||
		    emit_common_values(stage, index, values, common_value_count))
			goto io;
	}
	first_operand = 0;
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct orlix_tcti_target_feature_sat_value *values;
		size_t value_count = 0;
		size_t operand_index;

		values = orlix_tcti_target_feature_sat_certificate_values(
			audit, index, &value_count);
		for (operand_index = common_value_count;
		     operand_index < value_count; operand_index++) {
			const struct orlix_tcti_target_feature_sat_value *value =
				&values[operand_index];

			if (fprintf(stage,
				"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_OPERAND_VALUE(%zuU, %zuU, ",
				first_operand++, index) < 0 ||
			    emit_c_string(stage, value->name) ||
			    fprintf(stage, ", %uU, 0x%016" PRIx64 "ULL, 0x%016" PRIx64
				"ULL)\n", (unsigned int)value->width,
				value->low, value->high) < 0)
				goto io;
		}
	}
	if (copy_staged(stage, output))
		goto io;
	fclose(stage);
	free(condition_lengths);
	free(condition_offsets);
	return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK;
io:
	fclose(stage);
	free(condition_lengths);
	free(condition_offsets);
	return ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_IO;
}
