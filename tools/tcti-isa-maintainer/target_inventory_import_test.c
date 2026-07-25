// SPDX-License-Identifier: GPL-2.0-only
#include "target_inventory_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
				    enum tcti_target_import_error_code expected)
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
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	int written;

	written = snprintf(json, sizeof(json), "%s%s%s", prefix, values, suffix);
	if (written < 0 || (size_t)written >= sizeof(json))
		return -1;
	EXPECT_EQ(-1, tcti_target_inventory_import(json, (size_t)written,
			&inventory, &error));
	EXPECT_EQ(expected, error.code);
	tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int malformed_json_fails(void)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };

	EXPECT_EQ(-1, tcti_target_inventory_import("{", 1, &inventory, &error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_INVALID_JSON, error.code);
	tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int invalid_primitive_fails(void)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	static const char json[] = "{\"bad\":nan}";

	EXPECT_EQ(-1, tcti_target_inventory_import(json, strlen(json),
			&inventory, &error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_INVALID_JSON, error.code);
	return 0;
}

static int duplicate_decoded_key_fails(void)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	static const char json[] = "{\"a\":1,\"\\u0061\":2}";

	EXPECT_EQ(-1, tcti_target_inventory_import(json, strlen(json),
			&inventory, &error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_DUPLICATE_KEY, error.code);
	return 0;
}

static int json_depth_limit_fails(void)
{
	char json[2 * 257 + 1];
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	size_t index;

	for (index = 0; index < 257; index++)
		json[index] = '[';
	for (index = 0; index < 257; index++)
		json[257 + index] = ']';
	json[514] = '\0';
	EXPECT_EQ(-1, tcti_target_inventory_import(json, 514, &inventory,
			&error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_DEPTH_LIMIT, error.code);
	return 0;
}

static int input_limit_fails(void)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	static const char json[] = "{}";

	EXPECT_EQ(-1, tcti_target_inventory_import(json,
			128U * 1024U * 1024U + 1U, &inventory, &error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_INPUT_LIMIT, error.code);
	return 0;
}

static const struct tcti_target_leaf *find_leaf(
	const struct tcti_target_inventory *inventory, const char *name,
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

static const struct tcti_target_operand *find_operand(
	const struct tcti_target_inventory *inventory, size_t leaf_index,
	const char *name)
{
	size_t cursor;

	for (cursor = 0; cursor < inventory->operand_count; cursor++)
		if (inventory->operands[cursor].leaf_index == leaf_index &&
		    !strcmp(inventory->operands[cursor].name, name))
			return &inventory->operands[cursor];
	return NULL;
}

static int expect_operand(const struct tcti_target_inventory *inventory,
			  size_t leaf_index, uint32_t condition, const char *name,
			  uint8_t start, uint8_t width, uint32_t variable_mask)
{
	const struct tcti_target_operand *operand =
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
	const struct tcti_target_inventory *inventory)
{
	const struct tcti_target_leaf *leaf;
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

static int pinned_leaf_source_spans_are_exact(
	const struct tcti_target_inventory *inventory, const char *json,
	size_t json_length)
{
	size_t index;

	for (index = 0; index < inventory->leaf_count; index++) {
		const struct tcti_target_leaf *leaf = &inventory->leaves[index];

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
	const struct tcti_target_inventory *inventory, const char *json,
	size_t json_length)
{
	size_t index;
	size_t other;
	bool duplicate_name = false;

	EXPECT_EQ(TCTI_A64_TARGET_INSTRUCTION_ALIAS_COUNT,
		  inventory->instruction_alias_count);
	EXPECT_EQ(TCTI_A64_TARGET_REACHABLE_OPERATION_ALIAS_COUNT,
		  inventory->reachable_operation_alias_count);
	for (index = 0; index < inventory->instruction_alias_count; index++) {
		const struct tcti_target_instruction_alias *alias =
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
	const struct tcti_target_inventory *inventory)
{
	size_t index;

	for (index = 0; index < inventory->instruction_alias_count; index++) {
		const struct tcti_target_instruction_alias *alias =
			&inventory->instruction_aliases[index];
		const struct tcti_target_expr *combined;
		const struct tcti_target_expr *parent;
		const struct tcti_target_expr *local;

		if (alias->condition >= inventory->expression_count)
			return -1;
		combined = &inventory->expressions[alias->condition];
		if (combined->kind != TCTI_TARGET_EXPR_AND ||
		    combined->left >= inventory->expression_count ||
		    combined->right >= inventory->expression_count)
			continue;
		parent = &inventory->expressions[combined->left];
		local = &inventory->expressions[combined->right];
		/* A local true alias condition must still retain its non-true parent. */
		if (parent->kind != TCTI_TARGET_EXPR_BOOL &&
		    local->kind == TCTI_TARGET_EXPR_BOOL && local->boolean)
			return 0;
	}
	fprintf(stderr, "no alias retained a nontrivial parent condition\n");
	return -1;
}

static char *operation_alias_target(char *json, size_t json_length,
	const struct tcti_target_operation *operation)
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
	const struct tcti_target_inventory *inventory)
{
	struct tcti_target_import_error error = {0};
	struct tcti_target_inventory mutated = {0};
	size_t index;

	for (index = 0; index < inventory->operation_count; index++) {
		const struct tcti_target_operation *operation =
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
		if (tcti_target_inventory_import(json, json_length, &mutated,
						 &error) != -1 ||
		    error.code != TCTI_TARGET_IMPORT_INVALID_SOURCE) {
			*target = original;
			tcti_target_inventory_destroy(&mutated);
			return -1;
		}
		*target = original;
		tcti_target_inventory_destroy(&mutated);
		return 0;
	}
	return -1;
}

static int reachable_operation_alias_cycle_is_rejected(
	char *json, size_t json_length,
	const struct tcti_target_inventory *inventory)
{
	struct tcti_target_import_error error = {0};
	struct tcti_target_inventory mutated = {0};
	size_t index;

	for (index = 0; index < inventory->operation_count; index++) {
		const struct tcti_target_operation *operation =
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
		if (tcti_target_inventory_import(json, json_length, &mutated,
						 &error) != -1 ||
		    error.code != TCTI_TARGET_IMPORT_INVALID_SOURCE) {
			memcpy(target, saved, target_length + 1);
			free(saved);
			tcti_target_inventory_destroy(&mutated);
			return -1;
		}
		memcpy(target, saved, target_length + 1);
		free(saved);
		tcti_target_inventory_destroy(&mutated);
		return 0;
	}
	return -1;
}

static int import_pinned_source(const char *path)
{
	FILE *file;
	long length;
	char *json;
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
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
	if (tcti_target_inventory_import(json, (size_t)length, &inventory,
					 &error)) {
		fprintf(stderr, "pinned import failed: %u at %zu: %s\n",
			error.code, error.offset, error.message);
		goto out_json;
	}
	EXPECT_EQ(TCTI_A64_TARGET_LEAF_COUNT, inventory.leaf_count);
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
	tcti_target_inventory_destroy(&inventory);
	memset(&error, 0, sizeof(error));
	json[length] = ' ';
	EXPECT_EQ(-1, tcti_target_inventory_import(json, (size_t)length + 1,
			&inventory, &error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_HASH_MISMATCH, error.code);
	status = 0;
out_json:
	free(json);
out_file:
	if (file)
		fclose(file);
	tcti_target_inventory_destroy(&inventory);
	return status;
}

static int source_metadata_mutation_fails(void)
{
	static const char mutated_source[] =
		"{\"_meta\":{\"version\":{\"architecture\":\"other\","
		"\"build\":\"818\",\"ref\":\"2026-06_rel\","
		"\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"instructions\":[]}";
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };

	EXPECT_EQ(-1, tcti_target_inventory_import(mutated_source,
			strlen(mutated_source), &inventory, &error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_INVALID_SOURCE, error.code);
	tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int source_leaf_count_mutation_fails(void)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };

	EXPECT_EQ(-1, tcti_target_inventory_import(empty_a64_source,
			strlen(empty_a64_source), &inventory, &error));
	EXPECT_EQ(TCTI_TARGET_IMPORT_COUNT_MISMATCH, error.code);
	tcti_target_inventory_destroy(&inventory);
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

	return import_encoding_mutation(values, TCTI_TARGET_IMPORT_INVALID_SOURCE);
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

	return import_encoding_mutation(values, TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

static int unnamed_variable_bits_fail(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Bits\","
		"\"range\":{\"start\":0,\"width\":5},"
		"\"value\":{\"value\":\"'xxxxx'\"}}]";

	return import_encoding_mutation(values, TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

static int overflowing_operand_range_fails(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rn\","
		"\"range\":{\"start\":31,\"width\":2},"
		"\"value\":{\"value\":\"'xx'\"}}]";

	return import_encoding_mutation(values, TCTI_TARGET_IMPORT_INVALID_SOURCE);
}

static int numeric_operand_range_overflow_fails(void)
{
	static const char values[] =
		"[{\"_type\":\"Instruction.Encodeset.Field\",\"name\":\"Rn\","
		"\"range\":{\"start\":4294967296,\"width\":1},"
		"\"value\":{\"value\":\"'x'\"}}]";

	return import_encoding_mutation(values, TCTI_TARGET_IMPORT_INVALID_SOURCE);
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
