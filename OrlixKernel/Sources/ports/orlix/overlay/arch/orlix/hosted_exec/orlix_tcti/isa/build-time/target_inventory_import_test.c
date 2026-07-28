// SPDX-License-Identifier: GPL-2.0-only
#include "target_inventory_import.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The production importer deliberately accepts only the pinned source.  This
 * test-local compilation uses the same C parser with only its final digest
 * comparison redirected, so a minimal in-memory source can exercise field
 * classification without weakening the production entry point.
 */
static const char *fixture_pinned_hash;

static int fixture_strcmp(const char *left, const char *right)
{
	if (right == fixture_pinned_hash)
		return 0;
	return strcmp(left, right);
}

const struct orlix_tcti_target_operation *fixture_inventory_operation(
	const struct orlix_tcti_target_inventory *inventory, const char *id);
void fixture_inventory_sha256(const void *data, size_t length, char digest[65]);

#define orlix_tcti_target_inventory_destroy fixture_inventory_destroy
#define orlix_tcti_target_inventory_import fixture_inventory_import
#define orlix_tcti_target_inventory_operation fixture_inventory_operation
#define orlix_tcti_target_inventory_sha256 fixture_inventory_sha256
#define source_sha256 fixture_source_sha256
#define strcmp fixture_strcmp
#include "target_inventory_import.c"
#undef strcmp
#undef source_sha256
#undef orlix_tcti_target_inventory_sha256
#undef orlix_tcti_target_inventory_operation
#undef orlix_tcti_target_inventory_import
#undef orlix_tcti_target_inventory_destroy

#define EXPECT_EQ(expected, actual)                                            \
	do {                                                                      \
		if ((expected) != (actual)) {                                        \
			fprintf(stderr, "%s:%d: expected %d, got %d\n", __FILE__,      \
				__LINE__, (int)(expected), (int)(actual));                  \
			return -1;                                                        \
		}                                                                     \
	} while (0)

static const char empty_a64_source[] =
	"{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\","
	"\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\","
	"\"timestamp\":\"2026-06-24 17:12:14\"}},"
	"\"instructions\":[{\"name\":\"A64\","
	"\"_type\":\"Instruction.InstructionSet\","
	"\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},"
	"\"children\":[]}]}";

static int import_encoding_mutation(const char *values,
				    enum orlix_tcti_target_import_error_code expected)
{
	static const char prefix[] =
		"{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\","
		"\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\","
		"\"timestamp\":\"2026-06-24 17:12:14\"}},"
		"\"instructions\":[{\"name\":\"A64\","
		"\"_type\":\"Instruction.InstructionSet\","
		"\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},"
		"\"children\":[{\"_type\":\"Instruction.Instruction\","
		"\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},"
		"\"name\":\"operand_mutation\",\"operation_id\":\"operand_mutation\","
		"\"assembly\":{\"symbols\":[{\"_type\":\"Instruction.Symbols.Literal\","
		"\"value\":\"OP\"}]},\"encoding\":{\"width\":32,\"values\":";
	static const char suffix[] = "}}]}]}";
	char json[4096];
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	int written;

	written = snprintf(json, sizeof(json), "%s%s%s", prefix, values, suffix);
	if (written < 0 || (size_t)written >= sizeof(json))
		return -1;
	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(json, (size_t)written,
			&inventory, &error));
	EXPECT_EQ(expected, error.code);
	orlix_tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int malformed_json_fails(void)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };

	EXPECT_EQ(-1, orlix_tcti_target_inventory_import("{", 1, &inventory, &error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_INVALID_JSON, error.code);
	orlix_tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int invalid_primitive_fails(void)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	static const char json[] = "{\"bad\":nan}";

	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(json, strlen(json),
			&inventory, &error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_INVALID_JSON, error.code);
	return 0;
}

static int duplicate_decoded_key_fails(void)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	static const char json[] = "{\"a\":1,\"\\u0061\":2}";

	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(json, strlen(json),
			&inventory, &error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_DUPLICATE_KEY, error.code);
	return 0;
}

static int json_depth_limit_fails(void)
{
	char json[2 * 257 + 1];
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	size_t index;

	for (index = 0; index < 257; index++)
		json[index] = '[';
	for (index = 0; index < 257; index++)
		json[257 + index] = ']';
	json[514] = '\0';
	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(json, 514, &inventory,
			&error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_DEPTH_LIMIT, error.code);
	return 0;
}

static int input_limit_fails(void)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	static const char json[] = "{}";

	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(json,
			128U * 1024U * 1024U + 1U, &inventory, &error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_INPUT_LIMIT, error.code);
	return 0;
}

static const struct orlix_tcti_target_leaf *find_leaf(
	const struct orlix_tcti_target_inventory *inventory, const char *name,
	size_t *index)
{
	size_t cursor;

	for (cursor = 0; cursor < inventory->leaf_count; cursor++)
		if (!strcmp(inventory->leaves[cursor].name, name)) {
			*index = cursor;
			return &inventory->leaves[cursor];
		}
	return NULL;
}

static const struct orlix_tcti_target_operand *find_operand(
	const struct orlix_tcti_target_inventory *inventory, size_t leaf_index,
	const char *name)
{
	size_t cursor;

	for (cursor = 0; cursor < inventory->operand_count; cursor++)
		if (inventory->operands[cursor].leaf_index == leaf_index &&
		    !strcmp(inventory->operands[cursor].name, name))
			return &inventory->operands[cursor];
	return NULL;
}

static const struct orlix_tcti_target_fixed_operand *find_fixed_operand(
	const struct orlix_tcti_target_inventory *inventory, size_t leaf_index,
	const char *name)
{
	size_t cursor;

	for (cursor = 0; cursor < inventory->fixed_operand_count; cursor++)
		if (inventory->fixed_operands[cursor].leaf_index == leaf_index &&
		    !strcmp(inventory->fixed_operands[cursor].name, name))
			return &inventory->fixed_operands[cursor];
	return NULL;
}

static const struct orlix_tcti_target_condition_operand *find_condition_operand(
	const struct orlix_tcti_target_inventory *inventory, size_t leaf_index,
	const char *name)
{
	size_t cursor;

	for (cursor = 0; cursor < inventory->condition_operand_count; cursor++)
		if (inventory->condition_operands[cursor].leaf_index == leaf_index &&
		    !strcmp(inventory->condition_operands[cursor].name, name))
			return &inventory->condition_operands[cursor];
	return NULL;
}

static int source_span_contains(const char *json, size_t offset, size_t length,
				const char *needle)
{
	size_t needle_length = strlen(needle);
	size_t cursor;

	if (needle_length > length)
		return 0;
	for (cursor = 0; cursor <= length - needle_length; cursor++)
		if (!memcmp(json + offset + cursor, needle, needle_length))
			return 1;
	return 0;
}

static int source_span_is_exact_object(const char *json, size_t offset,
				       size_t length)
{
	size_t cursor;
	unsigned int depth = 0;
	int in_string = 0;
	int escaped = 0;

	if (!length || json[offset] != '{')
		return 0;
	for (cursor = 0; cursor < length; cursor++) {
		char byte = json[offset + cursor];

		if (in_string) {
			if (escaped)
				escaped = 0;
			else if (byte == '\\')
				escaped = 1;
			else if (byte == '"')
				in_string = 0;
			continue;
		}
		if (byte == '"') {
			in_string = 1;
			continue;
		}
		if (byte == '{')
			depth++;
		else if (byte == '}') {
			if (!depth || --depth == 0)
				return !depth && cursor + 1 == length;
		}
	}
	return 0;
}

static int append_fixture(char **cursor, size_t *remaining, const char *format,
			  ...)
{
	va_list args;
	int written;

	va_start(args, format);
	written = vsnprintf(*cursor, *remaining, format, args);
	va_end(args);
	if (written < 0 || (size_t)written >= *remaining)
		return -1;
	*cursor += written;
	*remaining -= (size_t)written;
	return 0;
}

static int named_fixed_and_partial_fields_use_distinct_planes(void)
{
	static const char prefix[] =
		"{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\","
		"\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\","
		"\"timestamp\":\"2026-06-24 17:12:14\"}},"
		"\"instructions\":[{\"name\":\"A64\","
		"\"_type\":\"Instruction.InstructionSet\","
		"\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},"
		"\"children\":[";
	static const char first_values[] =
		"[{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"fixed\","
		"\"range\":{\"start\":22,\"width\":2},"
		"\"value\":{\"value\":\"'10'\"}},"
		"{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"mixed\","
		"\"range\":{\"start\":5,\"width\":3},"
		"\"value\":{\"value\":\"'x0x'\"}}]";
	static const char other_values[] =
		"[{\"_type\":\"Instruction.Encodeset.Bits\","
		"\"range\":{\"start\":0,\"width\":1},"
		"\"value\":{\"value\":\"'0'\"}}]";
	static const char suffix[] =
		"]}],\"operations\":{\"fixture_op\":{"
		"\"_type\":\"Instruction.Operation\"}}}";
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	const struct orlix_tcti_target_leaf *leaf;
	const struct orlix_tcti_target_fixed_operand *fixed;
	const struct orlix_tcti_target_operand *mixed;
	char *json;
	char *cursor;
	size_t remaining;
	size_t leaf_index;
	uint32_t index;
	int status = -1;

	json = malloc(2U * 1024U * 1024U);
	if (!json)
		return -1;
	cursor = json;
	remaining = 2U * 1024U * 1024U;
	if (append_fixture(&cursor, &remaining, "%s", prefix))
		goto out;
	for (index = 0; index < ORLIX_TCTI_A64_TARGET_LEAF_COUNT; index++) {
		if (append_fixture(&cursor, &remaining,
			"%s{\"_type\":\"Instruction.Instruction\","
			"\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},"
			"\"name\":\"fixture_leaf_%u\",\"operation_id\":\"fixture_op\","
			"\"preferred\":null,"
			"\"assembly\":{\"symbols\":[{\"_type\":"
			"\"Instruction.Symbols.Literal\",\"value\":\"OP\"}]},"
			"\"encoding\":{\"width\":32,\"values\":%s}}",
			index ? "," : "", index,
			index ? other_values : first_values))
			goto out;
	}
	if (append_fixture(&cursor, &remaining, "%s", suffix))
		goto out;
	fixture_pinned_hash = fixture_source_sha256;
	if (fixture_inventory_import(json, (size_t)(cursor - json), &inventory,
				     &error))
		goto out;
	leaf = find_leaf(&inventory, "fixture_leaf_0", &leaf_index);
	fixed = leaf ? find_fixed_operand(&inventory, leaf_index, "fixed") : NULL;
	mixed = leaf ? find_operand(&inventory, leaf_index, "mixed") : NULL;
	if (!leaf || !fixed || !mixed ||
	    fixed->start != 22U || fixed->width != 2U ||
	    fixed->fixed_mask != 0x00c00000U ||
	    fixed->fixed_value != 0x00800000U ||
	    mixed->start != 5U || mixed->width != 3U ||
	    mixed->variable_mask != 0x000000a0U ||
	    find_operand(&inventory, leaf_index, "fixed") ||
	    find_fixed_operand(&inventory, leaf_index, "mixed"))
		goto out;
	status = 0;
out:
	fixture_pinned_hash = NULL;
	fixture_inventory_destroy(&inventory);
	free(json);
	return status;
}

static int expect_operand(const struct orlix_tcti_target_inventory *inventory,
			  size_t leaf_index, uint32_t condition, const char *name,
			  uint8_t start, uint8_t width, uint32_t variable_mask)
{
	const struct orlix_tcti_target_operand *operand =
		find_operand(inventory, leaf_index, name);

	if (!operand) {
		fprintf(stderr, "missing operand %s\n", name);
		return -1;
	}
	EXPECT_EQ(leaf_index, operand->leaf_index);
	EXPECT_EQ(condition, operand->condition);
	EXPECT_EQ(start, operand->start);
	EXPECT_EQ(width, operand->width);
	EXPECT_EQ(variable_mask, operand->variable_mask);
	return 0;
}

static int pinned_operand_provenance_is_exact(
	const struct orlix_tcti_target_inventory *inventory)
{
	const struct orlix_tcti_target_leaf *leaf;
	size_t leaf_index;

	leaf = find_leaf(inventory, "ADD_32_addsub_imm", &leaf_index);
	if (!leaf) {
		fprintf(stderr, "missing ADD_32_addsub_imm leaf\n");
		return -1;
	}
	if (expect_operand(inventory, leaf_index, leaf->condition, "sh", 22, 1,
			   0x00400000U) ||
	    expect_operand(inventory, leaf_index, leaf->condition, "imm12", 10,
			   12, 0x003ffc00U) ||
	    expect_operand(inventory, leaf_index, leaf->condition, "Rn", 5, 5,
			   0x000003e0U) ||
	    expect_operand(inventory, leaf_index, leaf->condition, "Rd", 0, 5,
			   0x0000001fU))
		return -1;
	return 0;
}

/*
 * sve_int_log_imm declares opc in the shared parent encodeset.  Its three
 * terminal encodings bind the field to a fixed value, while their inherited
 * condition excludes the fourth value.  This is source provenance, not a
 * runtime operand domain.
 */
static int pinned_inherited_fixed_operand_provenance_is_exact(
	const struct orlix_tcti_target_inventory *inventory, const char *json,
	size_t json_length)
{
	static const struct {
		const char *leaf_name;
		uint32_t value;
		const char *literal;
	} cases[] = {
		{ "orr_z_zi_", 0U, "'00'" },
		{ "eor_z_zi_", 1U, "'01'" },
		{ "and_z_zi_", 2U, "'10'" },
	};
	size_t case_index;

	for (case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]);
	     case_index++) {
		const struct orlix_tcti_target_leaf *leaf;
		size_t leaf_index;
		const struct orlix_tcti_target_fixed_operand *fixed;
		size_t leaf_end;

		leaf = find_leaf(inventory, cases[case_index].leaf_name, &leaf_index);
		if (!leaf)
			return -1;
		fixed = find_fixed_operand(inventory, leaf_index, "opc");
		if (!fixed || fixed->condition != leaf->condition ||
		    fixed->start != 22U || fixed->width != 2U ||
		    fixed->fixed_mask != 0x00c00000U ||
		    fixed->fixed_value != cases[case_index].value << 22 ||
		    !fixed->source_length)
			return -1;
		leaf_end = leaf->source_offset + leaf->source_length;
		if (leaf->source_offset >= json_length ||
		    leaf->source_length > json_length - leaf->source_offset ||
		    fixed->source_offset < leaf->source_offset ||
		    fixed->source_offset >= leaf_end ||
		    fixed->source_length > leaf_end - fixed->source_offset ||
		    !source_span_is_exact_object(json, fixed->source_offset,
						 fixed->source_length) ||
		    !source_span_contains(json, fixed->source_offset,
					  fixed->source_length,
					  "\"_type\": \"Instruction.Encodeset.Field\"") ||
		    !source_span_contains(json, fixed->source_offset,
					  fixed->source_length, "\"name\": \"opc\"") ||
		    !source_span_contains(json, fixed->source_offset,
					  fixed->source_length, "\"start\": 22") ||
		    !source_span_contains(json, fixed->source_offset,
					  fixed->source_length, "\"width\": 2") ||
		    !source_span_contains(json, fixed->source_offset,
					  fixed->source_length, cases[case_index].literal))
			return -1;
		if (find_operand(inventory, leaf_index, "opc"))
			return -1;
	}
	return 0;
}

/*
 * A named field is represented in exactly one provenance plane.  The pinned
 * source contains both fixed fields and fields with a mix of x and literal
 * bits, so this is a negative fixture for the old conflated representation.
 */
static int fixed_and_partially_variable_fields_are_disjoint(
	const struct orlix_tcti_target_inventory *inventory)
{
	size_t index;
	int saw_partial_variable = 0;

	for (index = 0; index < inventory->fixed_operand_count; index++) {
		const struct orlix_tcti_target_fixed_operand *fixed =
			&inventory->fixed_operands[index];

		if (find_operand(inventory, fixed->leaf_index, fixed->name))
			return -1;
	}
	for (index = 0; index < inventory->operand_count; index++) {
		const struct orlix_tcti_target_operand *operand =
			&inventory->operands[index];
		uint32_t field_mask = operand->width == 32 ? UINT32_MAX :
			((1U << operand->width) - 1U) << operand->start;

		if (find_fixed_operand(inventory, operand->leaf_index, operand->name))
			return -1;
		if (operand->variable_mask != field_mask)
			saw_partial_variable = 1;
	}
	return saw_partial_variable ? 0 : -1;
}

static int pinned_leaf_source_spans_are_exact(
	const struct orlix_tcti_target_inventory *inventory, const char *json,
	size_t json_length)
{
	size_t index;

	for (index = 0; index < inventory->leaf_count; index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory->leaves[index];

		if (!leaf->source_length || leaf->source_offset >= json_length ||
		    leaf->source_length > json_length - leaf->source_offset ||
		    json[leaf->source_offset] != '{' ||
		    !strstr(json + leaf->source_offset, leaf->name) ||
		    !strstr(json + leaf->source_offset, leaf->operation_id))
			return -1;
	}
	return 0;
}

static int pinned_instruction_aliases_are_exact(
	const struct orlix_tcti_target_inventory *inventory, const char *json,
	size_t json_length)
{
	size_t index;
	size_t other;
	bool duplicate_name = false;

	EXPECT_EQ(ORLIX_TCTI_A64_TARGET_INSTRUCTION_ALIAS_COUNT,
		  inventory->instruction_alias_count);
	EXPECT_EQ(ORLIX_TCTI_A64_TARGET_REACHABLE_OPERATION_ALIAS_COUNT,
		  inventory->reachable_operation_alias_count);
	for (index = 0; index < inventory->instruction_alias_count; index++) {
		const struct orlix_tcti_target_instruction_alias *alias =
			&inventory->instruction_aliases[index];

		if (alias->ordinal != index || !alias->name || !alias->operation_id ||
		    !alias->canonical_operation_id || !alias->source_length ||
		    !alias->condition_source_length || !alias->preferred_source_length ||
		    alias->source_offset >= json_length ||
		    alias->source_length > json_length - alias->source_offset ||
		    alias->condition_source_offset >= json_length ||
		    alias->condition_source_length >
			json_length - alias->condition_source_offset ||
		    alias->preferred_source_offset >= json_length ||
		    alias->preferred_source_length >
			json_length - alias->preferred_source_offset ||
		    json[alias->source_offset] != '{' ||
		    json[alias->condition_source_offset] != '{')
			return -1;
		for (other = 0; other < index; other++)
			if (!strcmp(alias->name,
				    inventory->instruction_aliases[other].name)) {
				duplicate_name = true;
				if (alias->ordinal ==
				    inventory->instruction_aliases[other].ordinal)
					return -1;
			}
	}
	return duplicate_name ? 0 : -1;
}

static int alias_effective_condition_inherits_parent(
	const struct orlix_tcti_target_inventory *inventory)
{
	size_t index;

	for (index = 0; index < inventory->instruction_alias_count; index++) {
		const struct orlix_tcti_target_instruction_alias *alias =
			&inventory->instruction_aliases[index];
		const struct orlix_tcti_target_expr *combined;
		const struct orlix_tcti_target_expr *parent;
		const struct orlix_tcti_target_expr *local;

		if (alias->condition >= inventory->expression_count)
			return -1;
		combined = &inventory->expressions[alias->condition];
		if (combined->kind != ORLIX_TCTI_TARGET_EXPR_AND ||
		    combined->left >= inventory->expression_count ||
		    combined->right >= inventory->expression_count)
			continue;
		parent = &inventory->expressions[combined->left];
		local = &inventory->expressions[combined->right];
		/* A local true alias condition must still retain its non-true parent. */
		if (parent->kind != ORLIX_TCTI_TARGET_EXPR_BOOL &&
		    local->kind == ORLIX_TCTI_TARGET_EXPR_BOOL && local->boolean)
			return 0;
	}
	fprintf(stderr, "no alias retained a nontrivial parent condition\n");
	return -1;
}

static char *operation_alias_target(char *json, size_t json_length,
	const struct orlix_tcti_target_operation *operation)
{
	static const char prefix[] = "\"operation_id\": \"";
	char *start;
	char *end;

	if (!operation || !operation->is_alias ||
	    operation->source_offset >= json_length ||
	    operation->source_length > json_length - operation->source_offset)
		return NULL;
	start = strstr(json + operation->source_offset, prefix);
	if (!start || (size_t)(start - json) >= operation->source_offset +
		operation->source_length)
		return NULL;
	start += sizeof(prefix) - 1;
	end = strchr(start, '"');
	if (!end || (size_t)(end - json) >= operation->source_offset +
		operation->source_length)
		return NULL;
	return start;
}

static int reachable_operation_alias_rejection_is_bounded(
	char *json, size_t json_length,
	const struct orlix_tcti_target_inventory *inventory)
{
	struct orlix_tcti_target_import_error error = {0};
	struct orlix_tcti_target_inventory mutated = {0};
	size_t index;

	for (index = 0; index < inventory->operation_count; index++) {
		const struct orlix_tcti_target_operation *operation =
			&inventory->operations[index];
		char *target;
		char original;

		if (!operation->is_alias || !operation->canonical_operation_id)
			continue;
		target = operation_alias_target(json, json_length, operation);
		if (!target)
			return -1;
		original = *target;
		*target = original == 'Z' ? 'Y' : 'Z';
		if (orlix_tcti_target_inventory_import(json, json_length, &mutated,
						 &error) != -1 ||
		    error.code != ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE) {
			*target = original;
			orlix_tcti_target_inventory_destroy(&mutated);
			return -1;
		}
		*target = original;
		orlix_tcti_target_inventory_destroy(&mutated);
		return 0;
	}
	return -1;
}

static int reachable_operation_alias_cycle_is_rejected(
	char *json, size_t json_length,
	const struct orlix_tcti_target_inventory *inventory)
{
	struct orlix_tcti_target_import_error error = {0};
	struct orlix_tcti_target_inventory mutated = {0};
	size_t index;

	for (index = 0; index < inventory->operation_count; index++) {
		const struct orlix_tcti_target_operation *operation =
			&inventory->operations[index];
		char *target;
		char *saved;
		size_t target_length;

		if (!operation->is_alias || !operation->canonical_operation_id ||
		    strlen(operation->id) != strlen(operation->alias_operation_id))
			continue;
		target = operation_alias_target(json, json_length, operation);
		if (!target)
			return -1;
		target_length = strlen(operation->alias_operation_id);
		saved = malloc(target_length + 1);
		if (!saved)
			return -1;
		memcpy(saved, target, target_length + 1);
		memcpy(target, operation->id, target_length);
		if (orlix_tcti_target_inventory_import(json, json_length, &mutated,
						 &error) != -1 ||
		    error.code != ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE) {
			memcpy(target, saved, target_length + 1);
			free(saved);
			orlix_tcti_target_inventory_destroy(&mutated);
			return -1;
		}
		memcpy(target, saved, target_length + 1);
		free(saved);
		orlix_tcti_target_inventory_destroy(&mutated);
		return 0;
	}
	return -1;
}

static int import_pinned_source(const char *path)
{
	FILE *file;
	long length;
	char *json;
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	int status = -1;

	if (!path)
		return 0;
	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET))
		goto out_file;
	json = malloc((size_t)length + 1);
	if (!json || fread(json, 1, (size_t)length, file) != (size_t)length)
		goto out_json;
	if (orlix_tcti_target_inventory_import(json, (size_t)length, &inventory,
					 &error)) {
		fprintf(stderr, "pinned import failed: %u at %zu: %s\n",
			error.code, error.offset, error.message);
		goto out_json;
	}
	EXPECT_EQ(ORLIX_TCTI_A64_TARGET_LEAF_COUNT, inventory.leaf_count);
	{
		size_t leaf_index;
		const struct orlix_tcti_target_leaf *leaf =
			find_leaf(&inventory, "st1b_z_p_br_", &leaf_index);
		const struct orlix_tcti_target_condition_operand *opc;

		EXPECT_EQ(1, leaf != NULL);
		EXPECT_EQ(1249U, leaf_index);
		opc = find_condition_operand(&inventory, leaf_index, "opc");
		EXPECT_EQ(1, opc != NULL);
		EXPECT_EQ(leaf->condition, opc->condition);
		EXPECT_EQ(22U, opc->start);
		EXPECT_EQ(3U, opc->width);
		EXPECT_EQ(UINT32_C(0x00400000), opc->variable_mask);
	}
	EXPECT_EQ(15980U, inventory.operand_count);
	EXPECT_EQ(16675U, inventory.fixed_operand_count);
	/*
	 * AARCHMRS 2026-06 embeds only a typed placeholder for every concrete
	 * operation.  The importer must retain the exact raw members instead of
	 * allowing the enclosing operation object to masquerade as ASL semantics.
	 */
	for (size_t index = 0; index < inventory.operation_count; index++) {
		const struct orlix_tcti_target_operation *operation =
			&inventory.operations[index];

		if (operation->is_alias)
			continue;
		if (operation->semantic_body_state !=
			    ORLIX_TCTI_TARGET_OPERATION_BODY_PLACEHOLDER ||
		    operation->decode_state != ORLIX_TCTI_TARGET_OPERATION_DECODE_NULL ||
		    !operation->semantic_member_source_length ||
		    !operation->semantic_body_source_length ||
		    !operation->decode_member_source_length ||
		    !operation->decode_source_length ||
		    operation->semantic_body_source_offset >= (size_t)length ||
		    operation->semantic_body_source_length >
			(size_t)length - operation->semantic_body_source_offset ||
		    operation->decode_source_offset >= (size_t)length ||
		    operation->decode_source_length >
			(size_t)length - operation->decode_source_offset ||
		    memcmp(json + operation->semantic_body_source_offset,
			   "// Not specified", operation->semantic_body_source_length) ||
		    memcmp(json + operation->decode_source_offset, "null",
			   operation->decode_source_length) ||
		    strcmp(operation->semantic_body_sha256,
			   "28fb16d9885379aa6e05267c659d8b7dab31e819051d85e1dbde8347bc2fdce8") ||
		    strcmp(operation->decode_sha256,
			   "74234e98afe7498fb5daf1f36ac2d78acc339464f950703b8c019892f982b90b")) {
			fprintf(stderr, "operation semantics mismatch: %s body=%u decode=%u "
				"body-span=%zu decode-span=%zu\n", operation->id,
				(unsigned int)operation->semantic_body_state,
				(unsigned int)operation->decode_state,
				operation->semantic_body_source_length,
				operation->decode_source_length);
			goto out_json;
		}
	}
	if (pinned_leaf_source_spans_are_exact(&inventory, json, (size_t)length))
		goto out_json;
	if (pinned_instruction_aliases_are_exact(&inventory, json,
					       (size_t)length))
		goto out_json;
	if (alias_effective_condition_inherits_parent(&inventory))
		goto out_json;
	if (reachable_operation_alias_rejection_is_bounded(json, (size_t)length,
						   &inventory))
		goto out_json;
	if (reachable_operation_alias_cycle_is_rejected(json, (size_t)length,
						     &inventory))
		goto out_json;
	if (pinned_operand_provenance_is_exact(&inventory))
		goto out_json;
	if (pinned_inherited_fixed_operand_provenance_is_exact(
		    &inventory, json, (size_t)length))
		goto out_json;
	if (named_fixed_and_partial_fields_use_distinct_planes())
		goto out_json;
	if (fixed_and_partially_variable_fields_are_disjoint(&inventory))
		goto out_json;
	orlix_tcti_target_inventory_destroy(&inventory);
	memset(&error, 0, sizeof(error));
	json[length] = ' ';
	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(json, (size_t)length + 1,
			&inventory, &error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_HASH_MISMATCH, error.code);
	status = 0;
out_json:
	free(json);
out_file:
	if (file)
		fclose(file);
	orlix_tcti_target_inventory_destroy(&inventory);
	return status;
}

static int source_metadata_mutation_fails(void)
{
	static const char mutated_source[] =
		"{\"_meta\":{\"version\":{\"architecture\":\"other\","
		"\"build\":\"818\",\"ref\":\"2026-06_rel\","
		"\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"instructions\":[]}";
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };

	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(mutated_source,
			strlen(mutated_source), &inventory, &error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE, error.code);
	orlix_tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int source_leaf_count_mutation_fails(void)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };

	EXPECT_EQ(-1, orlix_tcti_target_inventory_import(empty_a64_source,
			strlen(empty_a64_source), &inventory, &error));
	EXPECT_EQ(ORLIX_TCTI_TARGET_IMPORT_COUNT_MISMATCH, error.code);
	orlix_tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int overlapping_operand_ranges_fail(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rn\","
		"\"range\":{\"start\":0,\"width\":5},"
		"\"value\":{\"value\":\"'xxxxx'\"}},"
		"{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rd\","
		"\"range\":{\"start\":4,\"width\":5},"
		"\"value\":{\"value\":\"'xxxxx'\"}}]";

	return import_encoding_mutation(values, ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

static int duplicate_operand_name_fails(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rn\","
		"\"range\":{\"start\":0,\"width\":5},"
		"\"value\":{\"value\":\"'xxxxx'\"}},"
		"{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rn\","
		"\"range\":{\"start\":5,\"width\":5},"
		"\"value\":{\"value\":\"'xxxxx'\"}}]";

	return import_encoding_mutation(values, ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

static int unnamed_variable_bits_fail(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Bits\","
		"\"range\":{\"start\":0,\"width\":5},"
		"\"value\":{\"value\":\"'xxxxx'\"}}]";

	return import_encoding_mutation(values, ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

static int overflowing_operand_range_fails(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rn\","
		"\"range\":{\"start\":31,\"width\":2},"
		"\"value\":{\"value\":\"'xx'\"}}]";

	return import_encoding_mutation(values, ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

static int numeric_operand_range_overflow_fails(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rn\","
		"\"range\":{\"start\":4294967296,\"width\":1},"
		"\"value\":{\"value\":\"'x'\"}}]";

	return import_encoding_mutation(values, ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

int main(int argc, char **argv)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "malformed_json_fails", malformed_json_fails },
		{ "invalid_primitive_fails", invalid_primitive_fails },
		{ "duplicate_decoded_key_fails", duplicate_decoded_key_fails },
		{ "json_depth_limit_fails", json_depth_limit_fails },
		{ "input_limit_fails", input_limit_fails },
		{ "source_metadata_mutation_fails", source_metadata_mutation_fails },
		{ "source_leaf_count_mutation_fails",
		  source_leaf_count_mutation_fails },
		{ "overlapping_operand_ranges_fail", overlapping_operand_ranges_fail },
		{ "duplicate_operand_name_fails", duplicate_operand_name_fails },
		{ "unnamed_variable_bits_fail", unnamed_variable_bits_fail },
		{ "overflowing_operand_range_fails", overflowing_operand_range_fails },
		{ "numeric_operand_range_overflow_fails",
		  numeric_operand_range_overflow_fails },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run()) {
			fprintf(stderr, "FAIL %s\n", tests[index].name);
			return 1;
		}
		printf("PASS %s\n", tests[index].name);
	}
	if (argc == 2 && import_pinned_source(argv[1])) {
		fprintf(stderr, "FAIL import_pinned_source\n");
		return 1;
	}
	if (argc > 2) {
		fprintf(stderr, "usage: %s [Instructions.json]\n", argv[0]);
		return 1;
	}
	printf("target inventory importer mutation tests: %zu passed\n",
	       sizeof(tests) / sizeof(tests[0]));
	return 0;
}
