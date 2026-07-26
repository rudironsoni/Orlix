/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Independently decode every TCND record emitted into the Instructions
 * artifact and compare it recursively with the imported Arm source AST.  This
 * is deliberately not a re-use of the serializer under test.
 */
#define TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MAIN
#include "target_instruction_artifact_generator.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static char *read_file(const char *path, size_t *length)
{
	FILE *file;
	long file_length;
	char *data;

	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) || (file_length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	data = malloc((size_t)file_length + 1U);
	if (!data || fread(data, 1, (size_t)file_length, file) != (size_t)file_length ||
	    fclose(file)) {
		free(data);
		return NULL;
	}
	data[file_length] = '\0';
	*length = (size_t)file_length;
	return data;
}

static int read_u32be(const uint8_t *data, size_t length, size_t *offset,
			      uint32_t *value)
{
	if (*offset > length || length - *offset < 4U)
		return -1;
	*value = ((uint32_t)data[*offset] << 24) |
		((uint32_t)data[*offset + 1U] << 16) |
		((uint32_t)data[*offset + 2U] << 8) |
		(uint32_t)data[*offset + 3U];
	*offset += 4U;
	return 0;
}

static int tag_for_expression(enum orlix_tcti_target_expr_kind kind, uint8_t *tag)
{
	switch (kind) {
	case ORLIX_TCTI_TARGET_EXPR_BOOL: *tag = ORLIX_TCTI_TARGET_CONDITION_BOOL; return 0;
	case ORLIX_TCTI_TARGET_EXPR_FEATURE: *tag = ORLIX_TCTI_TARGET_CONDITION_FEATURE; return 0;
	case ORLIX_TCTI_TARGET_EXPR_OPERAND: *tag = ORLIX_TCTI_TARGET_CONDITION_OPERAND; return 0;
	case ORLIX_TCTI_TARGET_EXPR_VALUE: *tag = ORLIX_TCTI_TARGET_CONDITION_VALUE; return 0;
	case ORLIX_TCTI_TARGET_EXPR_SET: *tag = ORLIX_TCTI_TARGET_CONDITION_SET; return 0;
	case ORLIX_TCTI_TARGET_EXPR_NOT: *tag = ORLIX_TCTI_TARGET_CONDITION_NOT; return 0;
	case ORLIX_TCTI_TARGET_EXPR_AND: *tag = ORLIX_TCTI_TARGET_CONDITION_AND; return 0;
	case ORLIX_TCTI_TARGET_EXPR_OR: *tag = ORLIX_TCTI_TARGET_CONDITION_OR; return 0;
	case ORLIX_TCTI_TARGET_EXPR_EQ: *tag = ORLIX_TCTI_TARGET_CONDITION_EQ; return 0;
	case ORLIX_TCTI_TARGET_EXPR_NE: *tag = ORLIX_TCTI_TARGET_CONDITION_NE; return 0;
	case ORLIX_TCTI_TARGET_EXPR_IN: *tag = ORLIX_TCTI_TARGET_CONDITION_IN; return 0;
	}
	return -1;
}

static int expression_has_feature(const struct orlix_tcti_target_inventory *inventory,
				  uint32_t index, size_t depth)
{
	const struct orlix_tcti_target_expr *expression;
	uint32_t item;

	if (depth >= ORLIX_TCTI_TARGET_CONDITION_MAX_DEPTH ||
	    index == ORLIX_TCTI_TARGET_EXPR_NONE || index >= inventory->expression_count)
		return -1;
	expression = &inventory->expressions[index];
	if (expression->kind == ORLIX_TCTI_TARGET_EXPR_FEATURE)
		return 1;
	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_SET:
		if (expression->first_item > inventory->set_item_count ||
		    expression->item_count > inventory->set_item_count - expression->first_item)
			return -1;
		for (item = 0; item < expression->item_count; item++) {
			int result = expression_has_feature(inventory,
				inventory->set_items[expression->first_item + item], depth + 1U);

			if (result)
				return result;
		}
		return 0;
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		return expression_has_feature(inventory, expression->left, depth + 1U);
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN: {
		int left = expression_has_feature(inventory, expression->left, depth + 1U);
		int right;

		if (left)
			return left;
		right = expression_has_feature(inventory, expression->right, depth + 1U);
		return right;
	}
	case ORLIX_TCTI_TARGET_EXPR_BOOL:
	case ORLIX_TCTI_TARGET_EXPR_FEATURE:
	case ORLIX_TCTI_TARGET_EXPR_OPERAND:
	case ORLIX_TCTI_TARGET_EXPR_VALUE:
		return 0;
	}
	return -1;
}

static int compare_record(const struct orlix_tcti_target_inventory *inventory,
			  uint32_t expression_index, const uint8_t *data,
			  size_t length, size_t *offset, size_t depth)
{
	const struct orlix_tcti_target_expr *expression;
	uint8_t expected_tag;
	uint8_t actual_tag;
	uint32_t payload_length;
	size_t payload_start;
	size_t payload_end;

	if (depth >= ORLIX_TCTI_TARGET_CONDITION_MAX_DEPTH ||
	    expression_index == ORLIX_TCTI_TARGET_EXPR_NONE ||
	    expression_index >= inventory->expression_count ||
	    *offset > length || length - *offset < 5U)
		return -1;
	expression = &inventory->expressions[expression_index];
	if (tag_for_expression(expression->kind, &expected_tag))
		return -1;
	actual_tag = data[(*offset)++];
	if (actual_tag != expected_tag ||
	    read_u32be(data, length, offset, &payload_length) ||
	    payload_length > length - *offset)
		return -1;
	payload_start = *offset;
	payload_end = payload_start + payload_length;
	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_BOOL:
		if (payload_length != 1U || data[payload_start] != (uint8_t)expression->boolean)
			return -1;
		*offset = payload_end;
		break;
	case ORLIX_TCTI_TARGET_EXPR_FEATURE:
	case ORLIX_TCTI_TARGET_EXPR_OPERAND:
	case ORLIX_TCTI_TARGET_EXPR_VALUE: {
		uint32_t text_length;
		size_t source_length;

		if (!expression->text || read_u32be(data, payload_end, offset, &text_length))
			return -1;
		source_length = strlen(expression->text);
		if (source_length > UINT32_MAX || text_length != source_length ||
		    text_length != payload_end - *offset ||
		    memcmp(data + *offset, expression->text, text_length))
			return -1;
		*offset = payload_end;
		return 0;
	}
	case ORLIX_TCTI_TARGET_EXPR_SET: {
		uint32_t item_count;
		uint32_t item;

		if (expression->first_item > inventory->set_item_count ||
		    expression->item_count > inventory->set_item_count - expression->first_item ||
		    read_u32be(data, payload_end, offset, &item_count) ||
		    item_count != expression->item_count)
			return -1;
		for (item = 0; item < item_count; item++)
			if (compare_record(inventory,
				inventory->set_items[expression->first_item + item], data,
				payload_end, offset, depth + 1U))
				return -1;
		break;
	}
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		if (compare_record(inventory, expression->left, data, payload_end,
				   offset, depth + 1U))
			return -1;
		break;
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN:
		if (compare_record(inventory, expression->left, data, payload_end,
				   offset, depth + 1U) ||
		    compare_record(inventory, expression->right, data, payload_end,
				   offset, depth + 1U))
			return -1;
		break;
	}
	if (*offset != payload_end)
		return -1;
	return 0;
}

static int compare_condition(const struct orlix_tcti_target_inventory *inventory,
			     uint32_t expression, const uint8_t *data, size_t length)
{
	static const uint8_t header[] = { 'T', 'C', 'N', 'D', 1U };
	size_t offset = sizeof(header);

	if (length > ORLIX_TCTI_TARGET_CONDITION_MAX_SERIALIZED_BYTES ||
	    length <= sizeof(header) || memcmp(data, header, sizeof(header)) ||
	    compare_record(inventory, expression, data, length, &offset, 0U))
		return -1;
	return offset == length ? 0 : -1;
}

static int all_pinned_conditions_roundtrip(const char *source, size_t source_length)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error import_error = { 0 };
	struct artifact_model model = { 0 };
	enum orlix_tcti_target_instruction_artifact_error error;
	size_t leaf_index;
	size_t checked_leaf_conditions = 0;
	size_t checked_operand_conditions = 0;
	size_t feature_leaf_conditions = 0;
	size_t feature_operand_conditions = 0;
	size_t *source_counts = NULL;
	size_t *source_cursors = NULL;
	const struct orlix_tcti_target_operand **source_ordered = NULL;
	size_t source_operand_index;

	CHECK(orlix_tcti_target_inventory_import(source, source_length, &inventory,
					 &import_error) == 0);
	CHECK(inventory.leaf_count == ORLIX_TCTI_A64_TARGET_LEAF_COUNT);
	error = build_model(&inventory, &model);
	CHECK(error == ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OK);
	source_counts = calloc(inventory.leaf_count, sizeof(*source_counts));
	source_cursors = calloc(inventory.leaf_count, sizeof(*source_cursors));
	source_ordered = calloc(inventory.operand_count, sizeof(*source_ordered));
	CHECK(source_counts != NULL && source_cursors != NULL &&
	      (inventory.operand_count == 0 || source_ordered != NULL));
	for (source_operand_index = 0; source_operand_index < inventory.operand_count;
	     source_operand_index++) {
		const struct orlix_tcti_target_operand *operand =
			&inventory.operands[source_operand_index];

		CHECK(operand->leaf_index < inventory.leaf_count);
		source_counts[operand->leaf_index]++;
	}
	for (leaf_index = 0; leaf_index < inventory.leaf_count; leaf_index++) {
		CHECK(source_counts[leaf_index] == model.leaves[leaf_index].operand_count);
		source_cursors[leaf_index] = model.leaves[leaf_index].operand_first;
	}
	for (source_operand_index = 0; source_operand_index < inventory.operand_count;
	     source_operand_index++) {
		const struct orlix_tcti_target_operand *operand =
			&inventory.operands[source_operand_index];
		size_t destination = source_cursors[operand->leaf_index]++;

		CHECK(destination < inventory.operand_count);
		source_ordered[destination] = operand;
	}
	for (leaf_index = 0; leaf_index < inventory.leaf_count; leaf_index++) {
		const struct orlix_tcti_target_leaf *source_leaf = &inventory.leaves[leaf_index];
		const struct artifact_leaf *leaf = &model.leaves[leaf_index];
		int has_feature;
		size_t operand_index;

		CHECK(leaf->condition_offset <= model.conditions.length);
		CHECK(leaf->condition_length <= model.conditions.length - leaf->condition_offset);
		if (compare_condition(&inventory, source_leaf->condition,
			model.conditions.data + leaf->condition_offset,
			leaf->condition_length)) {
			fprintf(stderr, "leaf condition mismatch: index=%zu name=%s condition=%" PRIu32 "\n",
				leaf_index, source_leaf->name, source_leaf->condition);
			return -1;
		}
		checked_leaf_conditions++;
		has_feature = expression_has_feature(&inventory, source_leaf->condition, 0U);
		CHECK(has_feature >= 0);
		if (has_feature)
			feature_leaf_conditions++;
		for (operand_index = leaf->operand_first;
		     operand_index < (size_t)leaf->operand_first + leaf->operand_count;
		     operand_index++) {
			const struct artifact_operand *operand = &model.operands[operand_index];
			const struct orlix_tcti_target_operand *source_operand =
				source_ordered[operand_index];
			CHECK(source_operand != NULL);
			CHECK(!strcmp(source_operand->name,
				      (const char *)model.strings.data + operand->name_offset));
			if (compare_condition(&inventory, source_operand->condition,
				model.conditions.data + operand->condition_offset,
				operand->condition_length)) {
				fprintf(stderr, "operand condition mismatch: leaf=%zu operand=%zu name=%s condition=%" PRIu32 "\n",
					leaf_index, operand_index, source_operand->name,
					source_operand->condition);
				return -1;
			}
			checked_operand_conditions++;
			has_feature = expression_has_feature(&inventory, source_operand->condition, 0U);
			CHECK(has_feature >= 0);
			if (has_feature)
				feature_operand_conditions++;
		}
	}
	CHECK(checked_leaf_conditions == ORLIX_TCTI_A64_TARGET_LEAF_COUNT);
	CHECK(checked_operand_conditions == inventory.operand_count);
	CHECK(feature_leaf_conditions != 0);
	CHECK(feature_operand_conditions != 0);
	printf("PASS instruction artifact TCND source-AST roundtrip: leaves=%zu operands=%zu feature-leaves=%zu feature-operands=%zu\n",
	       checked_leaf_conditions, checked_operand_conditions,
	       feature_leaf_conditions, feature_operand_conditions);
	free(source_ordered);
	free(source_cursors);
	free(source_counts);
	artifact_model_destroy(&model);
	orlix_tcti_target_inventory_destroy(&inventory);
	return 0;
}

int main(int argc, char **argv)
{
	char *source;
	size_t source_length;

	if (argc != 2) {
		fprintf(stderr, "usage: %s Instructions.json\n", argv[0]);
		return 2;
	}
	source = read_file(argv[1], &source_length);
	if (!source) {
		perror(argv[1]);
		return 1;
	}
	if (all_pinned_conditions_roundtrip(source, source_length)) {
		free(source);
		return 1;
	}
	free(source);
	return 0;
}
