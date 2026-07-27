/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_runtime_capability_cohort_artifact_generator.h"

#include "target_feature_model.h"
#include "target_inventory_import.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ORLIX_TCTI_COHORT_MAX_MEMBERSHIPS (ORLIX_TCTI_A64_TARGET_LEAF_COUNT * 64U)

struct cohort_membership {
	uint32_t leaf;
	uint32_t parameter;
	size_t source_offset;
	size_t source_length;
};

struct cohort_model {
	struct cohort_membership *memberships;
	size_t count;
	size_t capacity;
};

static int parameter_index(const struct orlix_tcti_feature_model *features,
	const char *name, uint32_t *index)
{
	size_t i;

	for (i = 0; i < features->parameter_count; i++)
		if (!strcmp(features->parameters[i].name, name)) {
			*index = (uint32_t)i;
			return 0;
		}
	return -1;
}

static int reserve_memberships(struct cohort_model *model)
{
	struct cohort_membership *items;
	size_t capacity;

	if (model->count < model->capacity)
		return 0;
	if (model->count >= ORLIX_TCTI_COHORT_MAX_MEMBERSHIPS)
		return -1;
	capacity = model->capacity ? model->capacity * 2U : 256U;
	if (capacity > ORLIX_TCTI_COHORT_MAX_MEMBERSHIPS)
		capacity = ORLIX_TCTI_COHORT_MAX_MEMBERSHIPS;
	items = realloc(model->memberships, capacity * sizeof(*items));
	if (!items)
		return -1;
	model->memberships = items;
	model->capacity = capacity;
	return 0;
}

static int add_member(struct cohort_model *model, uint32_t leaf,
	uint32_t parameter, size_t source_offset, size_t source_length)
{
	size_t i;

	/* Membership is per leaf. Multiple syntactic sites remain one candidate. */
	for (i = 0; i < model->count; i++)
		if (model->memberships[i].leaf == leaf &&
		    model->memberships[i].parameter == parameter)
			return 0;
	if (reserve_memberships(model))
		return -1;
	model->memberships[model->count++] = (struct cohort_membership) {
		.leaf = leaf, .parameter = parameter,
		.source_offset = source_offset, .source_length = source_length,
	};
	return 0;
}

static int collect_expression(const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_feature_model *features, struct cohort_model *model,
	uint32_t leaf, uint32_t expression, uint8_t *visited)
{
	const struct orlix_tcti_target_expr *node;
	uint32_t parameter;
	uint32_t i;

	if (expression == ORLIX_TCTI_TARGET_EXPR_NONE)
		return 0;
	if (expression >= inventory->expression_count || visited[expression])
		return -1;
	visited[expression] = 1;
	node = &inventory->expressions[expression];
	if (node->kind == ORLIX_TCTI_TARGET_EXPR_FEATURE) {
		if (!node->text || parameter_index(features, node->text, &parameter))
			goto unknown_feature;
		if (add_member(model, leaf, parameter, node->source_offset,
				       node->source_length))
			goto invalid;
		visited[expression] = 0;
		return 0;
	}
	if (node->kind > ORLIX_TCTI_TARGET_EXPR_IN)
		goto invalid;
	if (node->left != ORLIX_TCTI_TARGET_EXPR_NONE &&
	    collect_expression(inventory, features, model, leaf, node->left, visited))
		goto invalid;
	if (node->right != ORLIX_TCTI_TARGET_EXPR_NONE &&
	    collect_expression(inventory, features, model, leaf, node->right, visited))
		goto invalid;
	if (node->first_item != ORLIX_TCTI_TARGET_EXPR_NONE) {
		if (node->first_item > inventory->set_item_count ||
		    node->item_count > inventory->set_item_count - node->first_item)
			goto invalid;
		for (i = 0; i < node->item_count; i++)
			if (collect_expression(inventory, features, model, leaf,
				inventory->set_items[node->first_item + i], visited))
				goto invalid;
	}
	visited[expression] = 0;
	return 0;
unknown_feature:
	visited[expression] = 0;
	return -2;
invalid:
	visited[expression] = 0;
	return -1;
}

static int membership_compare(const void *left, const void *right)
{
	const struct cohort_membership *a = left;
	const struct cohort_membership *b = right;

	return a->parameter < b->parameter ? -1 : a->parameter > b->parameter;
}

static int emit_string(FILE *output, const char *text)
{
	const unsigned char *p = (const unsigned char *)text;

	if (fputc('"', output) == EOF)
		return -1;
	while (*p) {
		if (*p == '"' || *p == '\\') {
			if (fputc('\\', output) == EOF || fputc(*p, output) == EOF)
				return -1;
		} else if (*p >= 0x20U && *p <= 0x7eU) {
			if (fputc(*p, output) == EOF)
				return -1;
		} else if (fprintf(output, "\\%03o", (unsigned int)*p) < 0) {
			return -1;
		}
		p++;
	}
	return fputc('"', output) == EOF ? -1 : 0;
}

enum orlix_tcti_runtime_capability_cohort_artifact_generator_error
orlix_tcti_runtime_capability_cohort_artifact_emit(const char *instructions,
	size_t instructions_length, const char *feature_source,
	size_t features_length, FILE *output)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_feature_model features = { 0 };
	struct orlix_tcti_target_import_error instruction_error = { 0 };
	struct orlix_tcti_feature_error feature_error = { 0 };
	struct cohort_model model = { 0 };
	uint8_t *visited = NULL;
	size_t leaf;
	size_t emitted_memberships = 0;
	enum orlix_tcti_runtime_capability_cohort_artifact_generator_error result =
		ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_OK;

	if (!instructions || !instructions_length || !feature_source ||
	    !features_length || !output)
		return ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INVALID_ARGUMENT;
	if (orlix_tcti_target_inventory_import(instructions, instructions_length,
					 &inventory, &instruction_error))
		return ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INSTRUCTION_IMPORT;
	if (orlix_tcti_target_feature_model_import(feature_source, features_length,
					    &features, &feature_error)) {
		result = ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_FEATURE_IMPORT;
		goto out;
	}
	if (inventory.leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
	    features.parameter_count != 409U) {
		result = ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_COUNT_MISMATCH;
		goto out;
	}
	visited = calloc(inventory.expression_count, 1U);
	if (!visited) {
		result = ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_NO_MEMORY;
		goto out;
	}
	for (leaf = 0; leaf < inventory.leaf_count; leaf++) {
		size_t first = model.count;
		size_t operand;
		int collect;

		memset(visited, 0, inventory.expression_count);
		collect = collect_expression(&inventory, &features, &model,
			(uint32_t)leaf, inventory.leaves[leaf].condition, visited);
		if (collect) {
			result = collect == -2 ?
				ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_UNKNOWN_FEATURE :
				ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INVALID_EXPRESSION;
			goto out;
		}
		for (operand = 0; operand < inventory.operand_count; operand++) {
			if (inventory.operands[operand].leaf_index != leaf)
				continue;
			memset(visited, 0, inventory.expression_count);
			collect = collect_expression(&inventory, &features, &model,
				(uint32_t)leaf, inventory.operands[operand].condition, visited);
			if (collect) {
				result = collect == -2 ?
					ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_UNKNOWN_FEATURE :
					ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INVALID_EXPRESSION;
				goto out;
			}
		}
		qsort(model.memberships + first, model.count - first,
		      sizeof(*model.memberships), membership_compare);
	}
	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated by target_runtime_capability_cohort_artifact_generator.c. Do not edit. */\n"
		  "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE(\"vFATAp1-A\", \"818\", \"2026-06_rel\", \"2.9.5\", "
		  "\"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\", ", output) == EOF ||
		fprintf(output, "%zuU, \"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187\", %zuU)\n",
			instructions_length, features_length) < 0 ||
		fprintf(output, "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(%zuU, %zuU, %zuU, %zuU)\n",
			inventory.leaf_count, model.count, model.count, model.count) < 0) {
		result = ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_IO;
		goto out;
	}
	for (leaf = 0; leaf < inventory.leaf_count; leaf++) {
		size_t count = 0, i;

		for (i = 0; i < model.count; i++)
			if (model.memberships[i].leaf == leaf)
				count++;
		if (fprintf(output, "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(%zuU, %zuU, %zuU)\n",
			leaf, emitted_memberships, count) < 0) {
			result = ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_IO;
			goto out;
		}
		emitted_memberships += count;
	}
	if (emitted_memberships != model.count) {
		result =
			ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_COUNT_MISMATCH;
		goto out;
	}
	for (leaf = 0; leaf < model.count; leaf++) {
		const struct cohort_membership *membership = &model.memberships[leaf];
		if (fprintf(output, "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(%zuU, %" PRIu32 "U, %" PRIu32 "U, ",
			leaf, membership->leaf, membership->parameter) < 0 ||
			emit_string(output, features.parameters[membership->parameter].name) ||
			fprintf(output, ", ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED, %zuU, %zuU)\n", membership->source_offset,
				membership->source_length) < 0) {
			result = ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_IO;
			goto out;
		}
	}
out:
	free(visited);
	free(model.memberships);
	orlix_tcti_target_feature_model_destroy(&features);
	orlix_tcti_target_inventory_destroy(&inventory);
	return result;
}

const char *orlix_tcti_runtime_capability_cohort_artifact_generator_error_name(
	enum orlix_tcti_runtime_capability_cohort_artifact_generator_error error)
{
	switch (error) {
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_OK: return "ok";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INSTRUCTION_IMPORT: return "instruction import failed";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_FEATURE_IMPORT: return "feature import failed";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_UNKNOWN_FEATURE: return "unknown feature reference";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INVALID_EXPRESSION: return "invalid expression";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_COUNT_MISMATCH: return "pinned count mismatch";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_NO_MEMORY: return "out of memory";
	case ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_IO: return "I/O failure";
	}
	return "unknown error";
}

#ifndef TARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_NO_MAIN
static int read_source(const char *path, char **data, size_t *length)
{
	FILE *file;
	long size;

	*data = NULL;
	*length = 0;
	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return -1;
	}
	*data = malloc((size_t)size + 1U);
	if (!*data || fread(*data, 1, (size_t)size, file) != (size_t)size ||
	    fclose(file)) {
		free(*data);
		*data = NULL;
		return -1;
	}
	(*data)[size] = '\0';
	*length = (size_t)size;
	return 0;
}

int main(int argc, char **argv)
{
	char *instructions = NULL, *features = NULL;
	size_t instructions_length, features_length;
	enum orlix_tcti_runtime_capability_cohort_artifact_generator_error error;

	if (argc != 3 || read_source(argv[1], &instructions, &instructions_length) ||
	    read_source(argv[2], &features, &features_length)) {
		fprintf(stderr, "usage: %s Instructions.json Features.json\n", argv[0]);
		free(instructions);
		free(features);
		return EXIT_FAILURE;
	}
	error = orlix_tcti_runtime_capability_cohort_artifact_emit(instructions,
		instructions_length, features, features_length, stdout);
	free(instructions);
	free(features);
	if (error != ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_OK) {
		fprintf(stderr, "runtime capability cohort generator: %s\n",
			orlix_tcti_runtime_capability_cohort_artifact_generator_error_name(error));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
#endif
