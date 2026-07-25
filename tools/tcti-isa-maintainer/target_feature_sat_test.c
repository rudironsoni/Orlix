/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_sat.h"
#include "target_inventory_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define EXPECT(value) do { if (!(value)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); return -1; \
} } while (0)

static int run(const struct tcti_feature_model *model,
	const struct tcti_target_inventory *inventory,
	struct tcti_target_feature_sat_audit *audit,
	struct tcti_target_feature_sat_error *error)
{
	memset(audit, 0, sizeof(*audit));
	memset(error, 0, sizeof(*error));
	return tcti_target_feature_sat_audit(model, inventory,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, audit, error);
}

static int run_with_limits(const struct tcti_feature_model *model,
	const struct tcti_target_inventory *inventory, size_t branches,
	size_t allocations, struct tcti_target_feature_sat_audit *audit,
	struct tcti_target_feature_sat_error *error)
{
	memset(audit, 0, sizeof(*audit));
	memset(error, 0, sizeof(*error));
	return tcti_target_feature_sat_audit(model, inventory, branches,
		allocations, audit, error);
}

static int boolean_union(void)
{
	static char a[] = "A", b[] = "B";
	static struct tcti_feature_parameter parameters[] = { { .name = a }, { .name = b } };
	static struct tcti_feature_node nodes[] = {
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = b },
		{ .kind = TCTI_FEATURE_IMPLIES, .left = 0, .right = 1 },
	};
	static uint32_t constraints[] = { 2 };
	static struct tcti_target_expr expressions[] = {
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = b },
		{ .kind = TCTI_TARGET_EXPR_AND, .left = 0, .right = 1 },
		{ .kind = TCTI_TARGET_EXPR_BOOL, .boolean = false },
	};
	static struct tcti_target_leaf leaves[] = {
		{ .condition = 0 }, { .condition = 1 }, { .condition = 2 }, { .condition = 3 },
	};
	struct tcti_feature_model model = { .parameters = parameters, .parameter_count = 2,
		.nodes = nodes, .node_count = 3, .constraints = constraints, .constraint_count = 1 };
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 4,
		.expressions = expressions, .expression_count = 4 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(audit.leaves[0] == TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[1] == TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[2] == TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[3] == TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	EXPECT(audit.parameter_count == 2);
	EXPECT(tcti_target_feature_sat_witness(&audit, 0));
	EXPECT(tcti_target_feature_sat_witness(&audit, 3) == NULL);
	tcti_target_feature_sat_audit_destroy(&audit);
	return 0;
}

static int deterministic_boolean_witnesses(void)
{
	static char a[] = "A", b[] = "B", c[] = "C";
	static struct tcti_feature_parameter parameters[] = {
		{ .name = a }, { .name = b }, { .name = c },
	};
	static struct tcti_target_expr expressions[] = {
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = b },
		{ .kind = TCTI_TARGET_EXPR_OR, .left = 0, .right = 1 },
		{ .kind = TCTI_TARGET_EXPR_NOT, .left = 0 },
		{ .kind = TCTI_TARGET_EXPR_BOOL, .boolean = false },
	};
	static struct tcti_target_leaf leaves[] = {
		{ .condition = 2 }, { .condition = 3 }, { .condition = 4 },
	};
	struct tcti_feature_model model = {
		.parameters = parameters, .parameter_count = 3,
	};
	struct tcti_target_inventory inventory = {
		.leaves = leaves, .leaf_count = 3,
		.expressions = expressions, .expression_count = 5,
	};
	struct tcti_target_feature_sat_audit first, second;
	struct tcti_target_feature_sat_error error;
	const signed char *witness;

	EXPECT(!run(&model, &inventory, &first, &error));
	EXPECT(!run(&model, &inventory, &second, &error));
	EXPECT(first.parameter_count == 3 && second.parameter_count == 3);
	EXPECT(!memcmp(first.witnesses, second.witnesses,
		first.leaf_count * first.parameter_count * sizeof(*first.witnesses)));
	witness = tcti_target_feature_sat_witness(&first, 0);
	EXPECT(witness && witness[0] == 1 && witness[1] == 1 && witness[2] == 1);
	witness = tcti_target_feature_sat_witness(&first, 1);
	EXPECT(witness && witness[0] == -1 && witness[1] == 1 && witness[2] == 1);
	EXPECT(tcti_target_feature_sat_witness(&first, 2) == NULL);
	EXPECT(!first.witnesses[2 * first.parameter_count]);
	EXPECT(!first.witnesses[2 * first.parameter_count + 1]);
	EXPECT(!first.witnesses[2 * first.parameter_count + 2]);
	tcti_target_feature_sat_audit_destroy(&second);
	tcti_target_feature_sat_audit_destroy(&first);
	return 0;
}

static int rejects_unsatisfiable_base(void)
{
	static char a[] = "A";
	static struct tcti_feature_parameter parameters[] = { { .name = a } };
	static struct tcti_feature_node nodes[] = {
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = TCTI_FEATURE_NOT, .left = 0 },
	};
	static uint32_t constraints[] = { 0, 1 };
	static struct tcti_target_expr expressions[] = { { .kind = TCTI_TARGET_EXPR_FEATURE, .text = a } };
	static struct tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 2, .constraints = constraints, .constraint_count = 2 };
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_UNSAT_BASE);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int rejects_unsupported_semantics(void)
{
	static char a[] = "A";
	static struct tcti_feature_parameter parameters[] = { { .name = a } };
	static struct tcti_feature_node nodes[] = {
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = TCTI_FEATURE_UINT, .first_child = 0, .child_count = 1 },
	};
	static uint32_t children[] = { 0 };
	static uint32_t constraints[] = { 1 };
	static struct tcti_target_expr expressions[] = { { .kind = TCTI_TARGET_EXPR_FEATURE, .text = a } };
	static struct tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 2, .constraints = constraints, .constraint_count = 1,
		.children = children, .child_count = 1 };
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int supports_409_boolean_parameters(void)
{
	struct tcti_feature_parameter *parameters;
	struct tcti_feature_node *nodes;
	uint32_t *constraints;
	struct tcti_target_expr expressions[2];
	struct tcti_target_leaf leaves[2];
	struct tcti_feature_model model;
	struct tcti_target_inventory inventory;
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;
	char (*names)[12];
	size_t index;

	parameters = calloc(409, sizeof(*parameters));
	nodes = calloc(1225, sizeof(*nodes));
	constraints = calloc(408, sizeof(*constraints));
	names = calloc(409, sizeof(*names));
	EXPECT(parameters && nodes && constraints && names);
	for (index = 0; index < 409; index++) {
		snprintf(names[index], sizeof(names[index]), "F%zu", index);
		parameters[index].name = names[index];
		nodes[index] = (struct tcti_feature_node) {
			.kind = TCTI_FEATURE_IDENTIFIER, .text = names[index],
		};
	}
	for (index = 0; index < 408; index++) {
		nodes[409 + index] = (struct tcti_feature_node) {
			.kind = TCTI_FEATURE_IMPLIES, .left = (uint32_t)index,
			.right = (uint32_t)(index + 1U),
		};
		constraints[index] = (uint32_t)(409 + index);
	}
	expressions[0] = (struct tcti_target_expr) { .kind = TCTI_TARGET_EXPR_FEATURE, .text = names[408] };
	expressions[1] = (struct tcti_target_expr) { .kind = TCTI_TARGET_EXPR_BOOL, .boolean = false };
	leaves[0] = (struct tcti_target_leaf) { .condition = 0 };
	leaves[1] = (struct tcti_target_leaf) { .condition = 1 };
	model = (struct tcti_feature_model) { .parameters = parameters, .parameter_count = 409,
		.nodes = nodes, .node_count = 817, .constraints = constraints, .constraint_count = 408 };
	inventory = (struct tcti_target_inventory) { .leaves = leaves, .leaf_count = 2,
		.expressions = expressions, .expression_count = 2 };
	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(audit.leaves[0] == TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[1] == TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	tcti_target_feature_sat_audit_destroy(&audit);
	free(names); free(constraints); free(nodes); free(parameters);
	return 0;
}

static int rejects_unused_unsupported_nodes(void)
{
	static char a[] = "A";
	static struct tcti_feature_parameter parameters[] = { { .name = a } };
	static struct tcti_feature_node nodes[] = {
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = TCTI_FEATURE_INTEGER, .integer = 7 },
	};
	static struct tcti_target_expr expressions[] = {
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = a },
	};
	static struct tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 2 };
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int rejects_unused_unsupported_target_expression(void)
{
	static char a[] = "A", value[] = "value";
	static struct tcti_feature_parameter parameters[] = { { .name = a } };
	static struct tcti_feature_node nodes[] = {
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = a },
	};
	static struct tcti_target_expr expressions[] = {
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = TCTI_TARGET_EXPR_VALUE, .text = value },
	};
	static struct tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 1 };
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 2 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int rejects_direct_input_cycles(void)
{
	static struct tcti_feature_node nodes[] = {
		{ .kind = TCTI_FEATURE_NOT, .left = 0 },
	};
	static struct tcti_target_expr expressions[] = {
		{ .kind = TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static struct tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct tcti_feature_model model = { .nodes = nodes, .node_count = 1 };
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	nodes[0] = (struct tcti_feature_node) { .kind = TCTI_FEATURE_BOOL, .integer = 1 };
	expressions[0] = (struct tcti_target_expr) {
		.kind = TCTI_TARGET_EXPR_NOT, .left = 0,
	};
	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	return 0;
}

static int rejects_literal_namespace_over_int_max(void)
{
	static struct tcti_target_expr expressions[] = {
		{ .kind = TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static struct tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct tcti_feature_model model = {
		.parameter_count = (size_t)INT_MAX + 1U,
	};
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int cumulative_resource_limits(void)
{
	static char a[] = "A", b[] = "B";
	static struct tcti_feature_parameter parameters[] = { { .name = a }, { .name = b } };
	static struct tcti_feature_node nodes[] = {
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = TCTI_FEATURE_IDENTIFIER, .text = b },
	};
	static struct tcti_target_expr expressions[] = {
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = TCTI_TARGET_EXPR_FEATURE, .text = b },
		{ .kind = TCTI_TARGET_EXPR_OR, .left = 0, .right = 1 },
	};
	static struct tcti_target_leaf leaves[] = {
		{ .condition = 2 }, { .condition = 2 }, { .condition = 2 },
	};
	struct tcti_feature_model model = { .parameters = parameters, .parameter_count = 2,
		.nodes = nodes, .node_count = 2 };
	struct tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 3,
		.expressions = expressions, .expression_count = 3 };
	struct tcti_target_inventory one_leaf_inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 3 };
	struct tcti_target_feature_sat_audit audit;
	struct tcti_target_feature_sat_error error;

	EXPECT(!run_with_limits(&model, &one_leaf_inventory, 4,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, &audit, &error));
	tcti_target_feature_sat_audit_destroy(&audit);
	EXPECT(run_with_limits(&model, &inventory, 4,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT);
	EXPECT(!audit.leaves && !audit.leaf_count);
	EXPECT(run_with_limits(&model, &inventory,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT, 1, &audit, &error));
	EXPECT(error.code == TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static char *read_file(const char *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;
	char *data;

	if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file) fclose(file);
		return NULL;
	}
	data = malloc((size_t)size + 1U);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);
	data[size] = '\0';
	*length = (size_t)size;
	return data;
}

static void print_pinned_census(const struct tcti_feature_model *model,
	const struct tcti_target_inventory *inventory)
{
	size_t feature_kinds[TCTI_FEATURE_VALUE + 1U] = {0};
	size_t target_kinds[TCTI_TARGET_EXPR_IN + 1U] = {0};
	size_t index;

	for (index = 0; index < model->node_count; index++)
		if (model->nodes[index].kind <= TCTI_FEATURE_VALUE)
			feature_kinds[model->nodes[index].kind]++;
	for (index = 0; index < inventory->expression_count; index++)
		if (inventory->expressions[index].kind <= TCTI_TARGET_EXPR_IN)
			target_kinds[inventory->expressions[index].kind]++;
	printf("pinned feature grammar: bool=%zu identifier=%zu integer=%zu dot-atom=%zu "
		"set=%zu not=%zu and=%zu or=%zu eq=%zu ne=%zu lt=%zu gt=%zu ge=%zu "
		"in=%zu implies=%zu iff=%zu uint=%zu sint=%zu field=%zu value=%zu\n",
		feature_kinds[TCTI_FEATURE_BOOL], feature_kinds[TCTI_FEATURE_IDENTIFIER],
		feature_kinds[TCTI_FEATURE_INTEGER], feature_kinds[TCTI_FEATURE_DOT_ATOM],
		feature_kinds[TCTI_FEATURE_SET], feature_kinds[TCTI_FEATURE_NOT],
		feature_kinds[TCTI_FEATURE_AND], feature_kinds[TCTI_FEATURE_OR],
		feature_kinds[TCTI_FEATURE_EQ], feature_kinds[TCTI_FEATURE_NE],
		feature_kinds[TCTI_FEATURE_LT], feature_kinds[TCTI_FEATURE_GT],
		feature_kinds[TCTI_FEATURE_GE], feature_kinds[TCTI_FEATURE_IN],
		feature_kinds[TCTI_FEATURE_IMPLIES], feature_kinds[TCTI_FEATURE_IFF],
		feature_kinds[TCTI_FEATURE_UINT], feature_kinds[TCTI_FEATURE_SINT],
		feature_kinds[TCTI_FEATURE_FIELD], feature_kinds[TCTI_FEATURE_VALUE]);
	printf("pinned target grammar: bool=%zu feature=%zu operand=%zu value=%zu set=%zu "
		"not=%zu and=%zu or=%zu eq=%zu ne=%zu in=%zu\n",
		target_kinds[TCTI_TARGET_EXPR_BOOL], target_kinds[TCTI_TARGET_EXPR_FEATURE],
		target_kinds[TCTI_TARGET_EXPR_OPERAND], target_kinds[TCTI_TARGET_EXPR_VALUE],
		target_kinds[TCTI_TARGET_EXPR_SET], target_kinds[TCTI_TARGET_EXPR_NOT],
		target_kinds[TCTI_TARGET_EXPR_AND], target_kinds[TCTI_TARGET_EXPR_OR],
		target_kinds[TCTI_TARGET_EXPR_EQ], target_kinds[TCTI_TARGET_EXPR_NE],
		target_kinds[TCTI_TARGET_EXPR_IN]);
}

static int pinned_integration(const char *features_path, const char *instructions_path)
{
	struct tcti_feature_model model = {0};
	struct tcti_target_inventory inventory = {0};
	struct tcti_feature_error feature_error = {0};
	struct tcti_target_import_error inventory_error = {0};
	struct tcti_target_feature_sat_audit audit = {0};
	struct tcti_target_feature_sat_error sat_error = {0};
	char *features;
	char *instructions;
	size_t features_length, instructions_length;
	int status = -1;

	features = read_file(features_path, &features_length);
	instructions = read_file(instructions_path, &instructions_length);
	EXPECT(features && instructions);
	EXPECT(!tcti_target_feature_model_import(features, features_length, &model,
		&feature_error));
	EXPECT(!tcti_target_inventory_import(instructions, instructions_length,
		&inventory, &inventory_error));
	EXPECT(model.parameter_count == TCTI_FEATURE_PARAMETER_COUNT);
	EXPECT(model.constraint_count == TCTI_FEATURE_CONSTRAINT_COUNT);
	EXPECT(inventory.leaf_count == TCTI_A64_TARGET_LEAF_COUNT);
	print_pinned_census(&model, &inventory);
	EXPECT(!tcti_target_feature_sat_audit(&model, &inventory,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, &audit, &sat_error));
	EXPECT(audit.leaf_count == TCTI_A64_TARGET_LEAF_COUNT);
	status = 0;
	tcti_target_feature_sat_audit_destroy(&audit);
	tcti_target_inventory_destroy(&inventory);
	tcti_target_feature_model_destroy(&model);
	free(instructions);
	free(features);
	return status;
}

int main(int argc, char **argv)
{
	EXPECT(!boolean_union());
	EXPECT(!deterministic_boolean_witnesses());
	EXPECT(!rejects_unsatisfiable_base());
	EXPECT(!rejects_unsupported_semantics());
	EXPECT(!supports_409_boolean_parameters());
	EXPECT(!rejects_unused_unsupported_nodes());
	EXPECT(!rejects_unused_unsupported_target_expression());
	EXPECT(!rejects_direct_input_cycles());
	EXPECT(!rejects_literal_namespace_over_int_max());
	EXPECT(!cumulative_resource_limits());
	if (argc == 3)
		EXPECT(!pinned_integration(argv[1], argv[2]));
	else
		EXPECT(argc == 1);
	puts("target feature SAT tests: passed");
	return 0;
}
