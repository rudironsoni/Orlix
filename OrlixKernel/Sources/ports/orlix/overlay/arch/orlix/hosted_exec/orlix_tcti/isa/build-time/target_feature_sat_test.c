/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_sat.h"
#include "target_inventory_import.h"
#include "target_register_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define EXPECT(value) do { if (!(value)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); return -1; \
} } while (0)

static int run(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory,
	struct orlix_tcti_target_feature_sat_audit *audit,
	struct orlix_tcti_target_feature_sat_error *error)
{
	memset(audit, 0, sizeof(*audit));
	memset(error, 0, sizeof(*error));
	return orlix_tcti_target_feature_sat_audit(model, inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, NULL, audit, error);
}

static int run_with_limits(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory, size_t branches,
	size_t allocations, struct orlix_tcti_target_feature_sat_audit *audit,
	struct orlix_tcti_target_feature_sat_error *error)
{
	memset(audit, 0, sizeof(*audit));
	memset(error, 0, sizeof(*error));
	return orlix_tcti_target_feature_sat_audit(model, inventory, branches,
		allocations, NULL, audit, error);
}

static int boolean_union(void)
{
	static char a[] = "A", b[] = "B";
	static struct orlix_tcti_feature_parameter parameters[] = { { .name = a }, { .name = b } };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = b },
		{ .kind = ORLIX_TCTI_FEATURE_IMPLIES, .left = 0, .right = 1 },
	};
	static uint32_t constraints[] = { 2 };
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = b },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_AND, .left = 0, .right = 1 },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = false },
	};
	static struct orlix_tcti_target_leaf leaves[] = {
		{ .condition = 0 }, { .condition = 1 }, { .condition = 2 }, { .condition = 3 },
	};
	struct orlix_tcti_feature_model model = { .parameters = parameters, .parameter_count = 2,
		.nodes = nodes, .node_count = 3, .constraints = constraints, .constraint_count = 1 };
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 4,
		.expressions = expressions, .expression_count = 4 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	if (run(&model, &inventory, &audit, &error)) {
		fprintf(stderr, "boolean_union: SAT error=%d leaf=%zu offset=%zu\n",
			error.code, error.leaf_index, error.provenance.offset);
		return -1;
	}
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[1] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[2] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[3] == ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	EXPECT(audit.parameter_count == 2);
	EXPECT(orlix_tcti_target_feature_sat_witness(&audit, 0));
	EXPECT(orlix_tcti_target_feature_sat_witness(&audit, 3) == NULL);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	return 0;
}

static int deterministic_boolean_witnesses(void)
{
	static char a[] = "A", b[] = "B", c[] = "C";
	static struct orlix_tcti_feature_parameter parameters[] = {
		{ .name = a }, { .name = b }, { .name = c },
	};
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = b },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_OR, .left = 0, .right = 1 },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_NOT, .left = 0 },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = false },
	};
	static struct orlix_tcti_target_leaf leaves[] = {
		{ .condition = 2 }, { .condition = 3 }, { .condition = 4 },
	};
	struct orlix_tcti_feature_model model = {
		.parameters = parameters, .parameter_count = 3,
	};
	struct orlix_tcti_target_inventory inventory = {
		.leaves = leaves, .leaf_count = 3,
		.expressions = expressions, .expression_count = 5,
	};
	struct orlix_tcti_target_feature_sat_audit first, second;
	struct orlix_tcti_target_feature_sat_error error;
	const signed char *witness;

	EXPECT(!run(&model, &inventory, &first, &error));
	EXPECT(!run(&model, &inventory, &second, &error));
	EXPECT(first.parameter_count == 3 && second.parameter_count == 3);
	EXPECT(!memcmp(first.witnesses, second.witnesses,
		first.leaf_count * first.parameter_count * sizeof(*first.witnesses)));
	witness = orlix_tcti_target_feature_sat_witness(&first, 0);
	EXPECT(witness && (witness[0] == 1 || witness[1] == 1));
	witness = orlix_tcti_target_feature_sat_witness(&first, 1);
	EXPECT(witness && witness[0] == -1);
	EXPECT(orlix_tcti_target_feature_sat_witness(&first, 2) == NULL);
	EXPECT(!first.witnesses[2 * first.parameter_count]);
	EXPECT(!first.witnesses[2 * first.parameter_count + 1]);
	EXPECT(!first.witnesses[2 * first.parameter_count + 2]);
	orlix_tcti_target_feature_sat_audit_destroy(&second);
	orlix_tcti_target_feature_sat_audit_destroy(&first);
	return 0;
}

static int rejects_unsatisfiable_base(void)
{
	static char a[] = "A";
	static struct orlix_tcti_feature_parameter parameters[] = { { .name = a } };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = ORLIX_TCTI_FEATURE_NOT, .left = 0 },
	};
	static uint32_t constraints[] = { 0, 1 };
	static struct orlix_tcti_target_expr expressions[] = { { .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a } };
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 2, .constraints = constraints, .constraint_count = 2 };
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSAT_BASE);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int rejects_unsupported_semantics(void)
{
	static char a[] = "A";
	static struct orlix_tcti_feature_parameter parameters[] = { { .name = a } };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 0, .child_count = 1 },
	};
	static uint32_t children[] = { 0 };
	static uint32_t constraints[] = { 1 };
	static struct orlix_tcti_target_expr expressions[] = { { .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a } };
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 2, .constraints = constraints, .constraint_count = 1,
		.children = children, .child_count = 1 };
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int supports_409_boolean_parameters(void)
{
	struct orlix_tcti_feature_parameter *parameters;
	struct orlix_tcti_feature_node *nodes;
	uint32_t *constraints;
	struct orlix_tcti_target_expr expressions[2];
	struct orlix_tcti_target_leaf leaves[2];
	struct orlix_tcti_feature_model model;
	struct orlix_tcti_target_inventory inventory;
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;
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
		nodes[index] = (struct orlix_tcti_feature_node) {
			.kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = names[index],
		};
	}
	for (index = 0; index < 408; index++) {
		nodes[409 + index] = (struct orlix_tcti_feature_node) {
			.kind = ORLIX_TCTI_FEATURE_IMPLIES, .left = (uint32_t)index,
			.right = (uint32_t)(index + 1U),
		};
		constraints[index] = (uint32_t)(409 + index);
	}
	expressions[0] = (struct orlix_tcti_target_expr) { .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = names[408] };
	expressions[1] = (struct orlix_tcti_target_expr) { .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = false };
	leaves[0] = (struct orlix_tcti_target_leaf) { .condition = 0 };
	leaves[1] = (struct orlix_tcti_target_leaf) { .condition = 1 };
	model = (struct orlix_tcti_feature_model) { .parameters = parameters, .parameter_count = 409,
		.nodes = nodes, .node_count = 817, .constraints = constraints, .constraint_count = 408 };
	inventory = (struct orlix_tcti_target_inventory) { .leaves = leaves, .leaf_count = 2,
		.expressions = expressions, .expression_count = 2 };
	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	EXPECT(audit.leaves[1] == ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	free(names); free(constraints); free(nodes); free(parameters);
	return 0;
}

static int accepts_unused_supported_numeric_nodes(void)
{
	static char a[] = "A";
	static struct orlix_tcti_feature_parameter parameters[] = { { .name = a } };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = ORLIX_TCTI_FEATURE_INTEGER, .integer = 7 },
	};
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 2 };
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_OK);
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	return 0;
}

static int exact_numeric_constants(void)
{
	static char one[] = "'1'", zero_one[] = "'01'";
	static uint32_t children[] = { 0, 2, 5, 7 };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_VALUE, .text = one },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 0, .child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_VALUE, .text = zero_one },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 1, .child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_EQ, .left = 1, .right = 3 },
		{ .kind = ORLIX_TCTI_FEATURE_VALUE, .text = one },
		{ .kind = ORLIX_TCTI_FEATURE_SINT, .first_child = 2, .child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_VALUE, .text = zero_one },
		{ .kind = ORLIX_TCTI_FEATURE_SINT, .first_child = 3, .child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_LT, .left = 6, .right = 8 },
	};
	static uint32_t constraints[] = { 4, 9 };
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = {
		.constraints = constraints, .constraint_count = 2,
		.nodes = nodes, .node_count = sizeof(nodes) / sizeof(nodes[0]),
		.children = children, .child_count = sizeof(children) / sizeof(children[0]),
	};
	struct orlix_tcti_target_inventory inventory = {
		.leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1,
	};
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_OK);
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	nodes[4].kind = ORLIX_TCTI_FEATURE_NE;
	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSAT_BASE);
	nodes[4].kind = ORLIX_TCTI_FEATURE_EQ;
	return 0;
}

static int inactive_field_domain_guard(void)
{
	static char use[] = "FEAT_USE", guard[] = "FEAT_GUARD";
	static char state[] = "AArch64", register_name[] = "LEGACY_ID";
	static char selector[] = "LEVEL", identifier_type[] = "AST.Identifier";
	static char value_type[] = "Values.Value", zero[] = "'00'", one[] = "'01'";
	static struct orlix_tcti_feature_parameter parameters[] = {
		{ .name = use }, { .name = guard },
	};
	static uint32_t children[] = { 3 };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = use },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = guard },
		{ .kind = ORLIX_TCTI_FEATURE_NOT, .left = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_FIELD, .field = {
			.state = state, .register_name = register_name,
			.selector = selector } },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 0,
			.child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_INTEGER, .integer = 2 },
		{ .kind = ORLIX_TCTI_FEATURE_GE, .left = 4, .right = 5 },
		{ .kind = ORLIX_TCTI_FEATURE_IMPLIES, .left = 0, .right = 6 },
	};
	static uint32_t constraints[] = { 2, 7 };
	static struct orlix_tcti_feature_field_domain_expression domain_expressions[] = {
		{ .type = identifier_type, .value = guard,
			.parent_expression = UINT32_MAX },
	};
	static struct orlix_tcti_feature_field_domain_node domains[] = {
		{ .type = value_type, .value = zero, .parent_domain = UINT32_MAX,
			.condition_expression = UINT32_MAX },
		{ .type = value_type, .value = one, .parent_domain = UINT32_MAX,
			.condition_expression = UINT32_MAX },
	};
	static struct orlix_tcti_feature_field_domain_value_candidate candidates[] = {
		{ .kind = ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_VALUES,
			.first_domain = 0, .domain_count = 2 },
	};
	static struct orlix_tcti_feature_field_domain_alternative alternatives[] = {
		{
			.field_condition_expression = 0,
			.wrapper_condition_expression = UINT32_MAX,
			.fieldset_condition_expression = UINT32_MAX,
			.relation_field_condition_expression = UINT32_MAX,
			.relation_wrapper_condition_expression = UINT32_MAX,
			.first_domain = 0, .domain_count = 2,
			.first_expression = 0, .expression_count = 1,
			.first_value_candidate = 0, .value_candidate_count = 1,
		},
	};
	static struct orlix_tcti_feature_field_domain_binding bindings[] = {
		{
			.feature_node_index = 3, .identity_group_index = 0,
			.occurrence_count = 1, .first_alternative = 0,
			.alternative_count = 1,
			.disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED,
			.domain = { .field_width = 2 },
		},
	};
	static struct orlix_tcti_target_expr target_expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = use },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = {
		.parameters = parameters, .parameter_count = 2,
		.constraints = constraints, .constraint_count = 2,
		.nodes = nodes, .node_count = sizeof(nodes) / sizeof(nodes[0]),
		.children = children, .child_count = 1,
	};
	struct orlix_tcti_target_inventory inventory = {
		.leaves = leaves, .leaf_count = 1,
		.expressions = target_expressions, .expression_count = 1,
	};
	struct orlix_tcti_feature_field_domain_bindings field_domains = {
		.items = bindings, .count = 1,
		.alternatives = alternatives, .alternative_count = 1,
		.domains = domains, .domain_count = 2,
		.expressions = domain_expressions, .expression_count = 1,
		.value_candidates = candidates, .value_candidate_count = 1,
	};
	struct orlix_tcti_target_feature_sat_audit audit = { 0 };
	struct orlix_tcti_target_feature_sat_error error = { 0 };
	const signed char *witness;
	const struct orlix_tcti_target_feature_sat_value *values;
	size_t value_count;

	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	witness = orlix_tcti_target_feature_sat_witness(&audit, 0);
	EXPECT(witness && witness[0] == 1 && witness[1] == -1);
	values = orlix_tcti_target_feature_sat_certificate_values(
		&audit, 0, &value_count);
	EXPECT(values && value_count == 1 && values[0].low >= 2);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	constraints[0] = 1;
	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	constraints[0] = 2;
	return 0;
}

static int finite_field_domain(void)
{
	static char feature[] = "FEAT_AA64EL1";
	static char state[] = "AArch64", register_name[] = "ID_AA64MMFR2_EL1";
	static char selector[] = "IESB", value_type[] = "Values.Value";
	static char zero[] = "'00'", one[] = "'01'";
	static struct orlix_tcti_feature_parameter parameters[] = { { .name = feature } };
	static uint32_t children[] = { 1 };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = feature },
		{ .kind = ORLIX_TCTI_FEATURE_FIELD,
		  .field = { .state = state, .register_name = register_name,
			.selector = selector } },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 0, .child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_INTEGER, .integer = 2 },
		{ .kind = ORLIX_TCTI_FEATURE_GE, .left = 2, .right = 3 },
		{ .kind = ORLIX_TCTI_FEATURE_NOT, .left = 4 },
		{ .kind = ORLIX_TCTI_FEATURE_IMPLIES, .left = 0, .right = 4 },
	};
	static uint32_t constraints[] = { 6 };
	static struct orlix_tcti_feature_field_domain_node domains[] = {
		{ .type = value_type, .value = zero,
		  .parent_domain = UINT32_MAX, .condition_expression = UINT32_MAX },
		{ .type = value_type, .value = one,
		  .parent_domain = UINT32_MAX, .condition_expression = UINT32_MAX },
	};
	static struct orlix_tcti_feature_field_domain_value_candidate value_candidates[] = {
		{ .kind = ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_VALUES,
		  .first_domain = 0, .domain_count = 2 },
	};
	static struct orlix_tcti_feature_field_domain_alternative alternatives[] = {
		{ .field_condition_expression = UINT32_MAX,
		  .wrapper_condition_expression = UINT32_MAX,
		  .fieldset_condition_expression = UINT32_MAX,
		  .relation_field_condition_expression = UINT32_MAX,
		  .relation_wrapper_condition_expression = UINT32_MAX,
		  .first_domain = 0, .domain_count = 2,
		  .first_value_candidate = 0, .value_candidate_count = 1 },
	};
	static struct orlix_tcti_feature_field_domain_binding bindings[] = {
		{ .feature_node_index = 1, .identity_group_index = 0,
		  .first_alternative = 0, .alternative_count = 1,
		  .disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED,
		  .domain = { .field_width = 2 } },
	};
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = {
		.parameters = parameters, .parameter_count = 1,
		.constraints = constraints, .constraint_count = 1,
		.nodes = nodes, .node_count = sizeof(nodes) / sizeof(nodes[0]),
		.children = children, .child_count = 1,
	};
	struct orlix_tcti_target_inventory inventory = {
		.leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1,
	};
	struct orlix_tcti_feature_field_domain_bindings field_domains = {
		.items = bindings, .count = 1,
		.alternatives = alternatives, .alternative_count = 1,
		.domains = domains, .domain_count = 2,
		.value_candidates = value_candidates, .value_candidate_count = 1,
	};
	struct orlix_tcti_target_feature_sat_audit audit = {0};
	struct orlix_tcti_target_feature_sat_error error = {0};

	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	expressions[0].kind = ORLIX_TCTI_TARGET_EXPR_FEATURE;
	expressions[0].text = feature;
	value_candidates[0].kind =
		ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_IMPLEMENTATION_DEFINED;
	value_candidates[0].domain_count = 0;
	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	value_candidates[0].kind = ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_VALUES;
	value_candidates[0].domain_count = 2;
	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	return 0;
}

static int exact_guarded_range_and_constraint_domains(void)
{
	static char feature[] = "FEAT_RAS";
	static char state[] = "AArch64", register_name[] = "REG";
	static char selector[] = "FIELD", range_type[] = "Values.ValueRange";
	static char two[] = "'10'", three[] = "'11'";
	static char identifier_type[] = "AST.Identifier";
	static char unsupported_type[] = "Values.EquationValue";
	static struct orlix_tcti_feature_parameter parameters[] = {
		{ .name = feature },
	};
	static uint32_t children[] = { 1 };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = feature },
		{ .kind = ORLIX_TCTI_FEATURE_FIELD,
		 .field = { .state = state, .register_name = register_name,
			.selector = selector } },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 0,
		 .child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_INTEGER, .integer = 2 },
		{ .kind = ORLIX_TCTI_FEATURE_GE, .left = 2, .right = 3 },
		{ .kind = ORLIX_TCTI_FEATURE_NOT, .left = 4 },
		{ .kind = ORLIX_TCTI_FEATURE_IMPLIES, .left = 0, .right = 5 },
	};
	static uint32_t constraints[] = { 6 };
	static struct orlix_tcti_feature_field_domain_expression expressions[] = {
		{ .type = identifier_type, .value = feature },
	};
	static struct orlix_tcti_feature_field_domain_node domains[] = {
		{ .type = range_type, .start = two, .end = three,
		 .parent_domain = UINT32_MAX,
		 .condition_expression = UINT32_MAX },
	};
	static struct orlix_tcti_feature_field_domain_value_candidate values[] = {
		{ .kind = ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_IMPLEMENTATION_DEFINED },
	};
	static struct orlix_tcti_feature_field_domain_constraint_candidate
	constraint_candidates[] = {
		{ .kind = ORLIX_TCTI_REGISTER_CONSTRAINT_VALUESET,
		 .first_domain = 0, .domain_count = 1 },
	};
	static struct orlix_tcti_feature_field_domain_alternative alternatives[] = {
		{
			.field_condition_expression = 0,
			.wrapper_condition_expression = UINT32_MAX,
			.fieldset_condition_expression = UINT32_MAX,
			.relation_field_condition_expression = UINT32_MAX,
			.relation_wrapper_condition_expression = UINT32_MAX,
			.first_domain = 0,
			.domain_count = 0,
			.first_expression = 0,
			.expression_count = 1,
			.first_value_candidate = 0,
			.value_candidate_count = 1,
		},
	};
	static struct orlix_tcti_feature_field_domain_binding bindings[] = {
		{
			.feature_node_index = 1,
			.identity_group_index = 0,
			.first_alternative = 0,
			.alternative_count = 1,
			.disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED,
			.domain = { .field_width = 2 },
		},
	};
	static struct orlix_tcti_target_expr target_expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = feature },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = {
		.parameters = parameters,
		.parameter_count = 1,
		.constraints = constraints,
		.constraint_count = 0,
		.nodes = nodes,
		.node_count = sizeof(nodes) / sizeof(nodes[0]),
		.children = children,
		.child_count = 1,
	};
	struct orlix_tcti_target_inventory inventory = {
		.leaves = leaves,
		.leaf_count = 1,
		.expressions = target_expressions,
		.expression_count = 1,
	};
	struct orlix_tcti_feature_field_domain_bindings field_domains = {
		.items = bindings,
		.count = 1,
		.alternatives = alternatives,
		.alternative_count = 1,
		.domains = domains,
		.domain_count = 1,
		.expressions = expressions,
		.expression_count = 1,
		.value_candidates = values,
		.value_candidate_count = 1,
		.constraint_candidates = constraint_candidates,
		.constraint_candidate_count = 1,
	};
	struct orlix_tcti_target_feature_sat_audit audit = { 0 };
	struct orlix_tcti_target_feature_sat_error error = { 0 };
	const struct orlix_tcti_target_feature_sat_certificate *certificate;
	const struct orlix_tcti_target_feature_sat_value *certificate_values;
	size_t certificate_value_count;

	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	certificate = orlix_tcti_target_feature_sat_certificate(&audit, 0);
	certificate_values = orlix_tcti_target_feature_sat_certificate_values(
		&audit, 0, &certificate_value_count);
	EXPECT(certificate && certificate->condition_index == 0);
	EXPECT(!certificate_values && certificate_value_count == 0);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);

	model.constraint_count = 1;
	alternatives[0].field_condition_expression = UINT32_MAX;
	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	certificate_values = orlix_tcti_target_feature_sat_certificate_values(
		&audit, 0, &certificate_value_count);
	EXPECT(certificate_values && certificate_value_count == 1);
	EXPECT(certificate_values[0].kind == ORLIX_TCTI_TARGET_FEATURE_SAT_FIELD);
	EXPECT(certificate_values[0].identity_group_index == 0);
	EXPECT(certificate_values[0].feature_node_index == UINT32_MAX);
	EXPECT(certificate_values[0].width == 2);
	EXPECT(certificate_values[0].low < 2);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);

	values[0].kind = ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_VALUES;
	values[0].first_domain = 0;
	values[0].domain_count = 1;
	alternatives[0].domain_count = 1;
	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	EXPECT(!orlix_tcti_target_feature_sat_certificate(&audit, 0));
	orlix_tcti_target_feature_sat_audit_destroy(&audit);

	values[0].kind =
		ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_IMPLEMENTATION_DEFINED;
	values[0].domain_count = 0;
	alternatives[0].first_constraint_candidate = 0;
	alternatives[0].constraint_candidate_count = 1;
	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);

	alternatives[0].constraint_candidate_count = 0;
	values[0].kind = ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_VALUES;
	values[0].domain_count = 1;
	domains[0].type = unsupported_type;
	EXPECT(orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
		&field_domains, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
	domains[0].type = range_type;
	return 0;
}

static int accepts_unused_supported_target_scalar(void)
{
	static char a[] = "A", value[] = "value";
	static struct orlix_tcti_feature_parameter parameters[] = { { .name = a } };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = a },
	};
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_VALUE, .text = value },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = { .parameters = parameters, .parameter_count = 1,
		.nodes = nodes, .node_count = 1 };
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 2 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_OK);
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	return 0;
}

static int exact_configuration_certificates(void)
{
	static char pmu[] = "PMU", pmdevid[] = "PMDEVID", version[] = "VERSION";
	static uint32_t children[] = { 0, 1, 2, 3 };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = pmu },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = pmdevid },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = version },
		{ .kind = ORLIX_TCTI_FEATURE_DOT_ATOM, .first_child = 0,
		 .child_count = 3 },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 3,
		 .child_count = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_INTEGER, .integer = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_GE, .left = 4, .right = 5 },
	};
	static uint32_t constraints[] = { 6 };
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = {
		.constraints = constraints,
		.constraint_count = 1,
		.nodes = nodes,
		.node_count = sizeof(nodes) / sizeof(nodes[0]),
		.children = children,
		.child_count = sizeof(children) / sizeof(children[0]),
	};
	struct orlix_tcti_target_inventory inventory = {
		.leaves = leaves,
		.leaf_count = 1,
		.expressions = expressions,
		.expression_count = 1,
	};
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;
	const struct orlix_tcti_target_feature_sat_certificate *certificate;
	const struct orlix_tcti_target_feature_sat_value *certificate_values;
	size_t certificate_value_count;

	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	certificate = orlix_tcti_target_feature_sat_certificate(&audit, 0);
	certificate_values = orlix_tcti_target_feature_sat_certificate_values(
		&audit, 0, &certificate_value_count);
	EXPECT(certificate && certificate->condition_index == 0);
	EXPECT(certificate_values && certificate_value_count == 1);
	EXPECT(certificate_values[0].kind ==
		ORLIX_TCTI_TARGET_FEATURE_SAT_CONFIGURATION);
	EXPECT(certificate_values[0].feature_node_index == 3);
	EXPECT(certificate_values[0].identity_group_index == UINT32_MAX);
	EXPECT(certificate_values[0].leaf_index == UINT32_MAX);
	EXPECT(!certificate_values[0].name);
	EXPECT(certificate_values[0].width == 128);
	EXPECT(certificate_values[0].low || certificate_values[0].high);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	return 0;
}

static int exact_target_operand_patterns(void)
{
	static char operand_name[] = "op", pattern[] = "'1x0'";
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_OPERAND, .text = operand_name },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_VALUE, .text = pattern },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_EQ, .left = 0, .right = 1 },
	};
	static struct orlix_tcti_target_leaf leaves[] = {
		{ .condition = 2, .encoding_pattern = 0U },
		{ .condition = 2, .encoding_pattern = 2U },
	};
	static struct orlix_tcti_target_operand operands[] = {
		{ .name = operand_name, .leaf_index = 0, .condition = 2,
		 .variable_mask = 5U, .start = 0, .width = 3 },
		{ .name = operand_name, .leaf_index = 1, .condition = 2,
		 .variable_mask = 5U, .start = 0, .width = 3 },
	};
	struct orlix_tcti_feature_model model = {0};
	struct orlix_tcti_target_inventory inventory = {
		.leaves = leaves, .leaf_count = 2,
		.expressions = expressions, .expression_count = 3,
		.operands = operands, .operand_count = 2,
	};
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;
	const struct orlix_tcti_target_feature_sat_certificate *certificate;
	const struct orlix_tcti_target_feature_sat_value *certificate_values;
	size_t certificate_value_count;

	EXPECT(!run(&model, &inventory, &audit, &error));
	EXPECT(audit.leaves[0] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	certificate = orlix_tcti_target_feature_sat_certificate(&audit, 0);
	certificate_values = orlix_tcti_target_feature_sat_certificate_values(
		&audit, 0, &certificate_value_count);
	EXPECT(certificate && certificate->condition_index == 2);
	EXPECT(certificate_values && certificate_value_count == 1);
	EXPECT(certificate_values[0].kind ==
		ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND);
	EXPECT(certificate_values[0].leaf_index == 0);
	EXPECT(certificate_values[0].feature_node_index == UINT32_MAX);
	EXPECT(certificate_values[0].name == operand_name);
	EXPECT(certificate_values[0].width == 3);
	EXPECT(certificate_values[0].low == 4U);
	EXPECT(audit.leaves[1] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE);
	certificate = orlix_tcti_target_feature_sat_certificate(&audit, 1);
	certificate_values = orlix_tcti_target_feature_sat_certificate_values(
		&audit, 1, &certificate_value_count);
	EXPECT(certificate && certificate->condition_index == 2);
	EXPECT(certificate_values && certificate_value_count == 1);
	EXPECT(certificate_values[0].kind ==
		ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND);
	EXPECT(certificate_values[0].leaf_index == 1);
	EXPECT(certificate_values[0].feature_node_index == UINT32_MAX);
	EXPECT(certificate_values[0].name == operand_name);
	EXPECT(certificate_values[0].width == 3);
	EXPECT(certificate_values[0].low == 6U);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	inventory.operand_count = 0;
	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
	return 0;
}

static int rejects_direct_input_cycles(void)
{
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_NOT, .left = 0 },
	};
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = { .nodes = nodes, .node_count = 1 };
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	nodes[0] = (struct orlix_tcti_feature_node) { .kind = ORLIX_TCTI_FEATURE_BOOL, .integer = 1 };
	expressions[0] = (struct orlix_tcti_target_expr) {
		.kind = ORLIX_TCTI_TARGET_EXPR_NOT, .left = 0,
	};
	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	return 0;
}

static int rejects_literal_namespace_over_int_max(void)
{
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static struct orlix_tcti_target_leaf leaves[] = { { .condition = 0 } };
	struct orlix_tcti_feature_model model = {
		.parameter_count = (size_t)INT_MAX + 1U,
	};
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 1 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(run(&model, &inventory, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	EXPECT(!audit.leaves && !audit.leaf_count);
	return 0;
}

static int cumulative_resource_limits(void)
{
	static char a[] = "A", b[] = "B";
	static struct orlix_tcti_feature_parameter parameters[] = { { .name = a }, { .name = b } };
	static struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = a },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = b },
	};
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = b },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_OR, .left = 0, .right = 1 },
	};
	static struct orlix_tcti_target_leaf leaves[] = {
		{ .condition = 2 }, { .condition = 2 }, { .condition = 2 },
	};
	struct orlix_tcti_feature_model model = { .parameters = parameters, .parameter_count = 2,
		.nodes = nodes, .node_count = 2 };
	struct orlix_tcti_target_inventory inventory = { .leaves = leaves, .leaf_count = 3,
		.expressions = expressions, .expression_count = 3 };
	struct orlix_tcti_target_inventory one_leaf_inventory = { .leaves = leaves, .leaf_count = 1,
		.expressions = expressions, .expression_count = 3 };
	struct orlix_tcti_target_feature_sat_audit audit;
	struct orlix_tcti_target_feature_sat_error error;

	EXPECT(!run_with_limits(&model, &one_leaf_inventory, 4,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, &audit, &error));
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	EXPECT(run_with_limits(&model, &inventory, 2,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT);
	EXPECT(!audit.leaves && !audit.leaf_count);
	EXPECT(run_with_limits(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT, 1, &audit, &error));
	EXPECT(error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT);
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

static void print_pinned_census(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory)
{
	size_t feature_kinds[ORLIX_TCTI_FEATURE_VALUE + 1U] = {0};
	size_t target_kinds[ORLIX_TCTI_TARGET_EXPR_IN + 1U] = {0};
	size_t index;

	for (index = 0; index < model->node_count; index++)
		if (model->nodes[index].kind <= ORLIX_TCTI_FEATURE_VALUE)
			feature_kinds[model->nodes[index].kind]++;
	for (index = 0; index < inventory->expression_count; index++)
		if (inventory->expressions[index].kind <= ORLIX_TCTI_TARGET_EXPR_IN)
			target_kinds[inventory->expressions[index].kind]++;
	printf("pinned feature grammar: bool=%zu identifier=%zu integer=%zu dot-atom=%zu "
		"set=%zu not=%zu and=%zu or=%zu eq=%zu ne=%zu lt=%zu gt=%zu ge=%zu "
		"in=%zu implies=%zu iff=%zu uint=%zu sint=%zu field=%zu value=%zu\n",
		feature_kinds[ORLIX_TCTI_FEATURE_BOOL], feature_kinds[ORLIX_TCTI_FEATURE_IDENTIFIER],
		feature_kinds[ORLIX_TCTI_FEATURE_INTEGER], feature_kinds[ORLIX_TCTI_FEATURE_DOT_ATOM],
		feature_kinds[ORLIX_TCTI_FEATURE_SET], feature_kinds[ORLIX_TCTI_FEATURE_NOT],
		feature_kinds[ORLIX_TCTI_FEATURE_AND], feature_kinds[ORLIX_TCTI_FEATURE_OR],
		feature_kinds[ORLIX_TCTI_FEATURE_EQ], feature_kinds[ORLIX_TCTI_FEATURE_NE],
		feature_kinds[ORLIX_TCTI_FEATURE_LT], feature_kinds[ORLIX_TCTI_FEATURE_GT],
		feature_kinds[ORLIX_TCTI_FEATURE_GE], feature_kinds[ORLIX_TCTI_FEATURE_IN],
		feature_kinds[ORLIX_TCTI_FEATURE_IMPLIES], feature_kinds[ORLIX_TCTI_FEATURE_IFF],
		feature_kinds[ORLIX_TCTI_FEATURE_UINT], feature_kinds[ORLIX_TCTI_FEATURE_SINT],
		feature_kinds[ORLIX_TCTI_FEATURE_FIELD], feature_kinds[ORLIX_TCTI_FEATURE_VALUE]);
	printf("pinned target grammar: bool=%zu feature=%zu operand=%zu value=%zu set=%zu "
		"not=%zu and=%zu or=%zu eq=%zu ne=%zu in=%zu\n",
		target_kinds[ORLIX_TCTI_TARGET_EXPR_BOOL], target_kinds[ORLIX_TCTI_TARGET_EXPR_FEATURE],
		target_kinds[ORLIX_TCTI_TARGET_EXPR_OPERAND], target_kinds[ORLIX_TCTI_TARGET_EXPR_VALUE],
		target_kinds[ORLIX_TCTI_TARGET_EXPR_SET], target_kinds[ORLIX_TCTI_TARGET_EXPR_NOT],
		target_kinds[ORLIX_TCTI_TARGET_EXPR_AND], target_kinds[ORLIX_TCTI_TARGET_EXPR_OR],
		target_kinds[ORLIX_TCTI_TARGET_EXPR_EQ], target_kinds[ORLIX_TCTI_TARGET_EXPR_NE],
		target_kinds[ORLIX_TCTI_TARGET_EXPR_IN]);
}

static void diagnose_unsat_prefix(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_feature_field_domain_bindings *field_domains)
{
	struct orlix_tcti_feature_parameter *parameters;
	struct orlix_tcti_feature_model subset = *model;
	struct orlix_tcti_target_expr expression = {
		.kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true,
	};
	struct orlix_tcti_target_leaf leaf = { .condition = 0 };
	struct orlix_tcti_target_inventory inventory = {
		.leaves = &leaf, .leaf_count = 1,
		.expressions = &expression, .expression_count = 1,
	};
	size_t low = 1, high = model->constraint_count, index;

	parameters = calloc(model->parameter_count, sizeof(*parameters));
	if (!parameters)
		return;
	for (index = 0; index < model->parameter_count; index++) {
		parameters[index] = model->parameters[index];
		parameters[index].first_constraint = 0;
		parameters[index].constraint_count = 0;
	}
	subset.parameters = parameters;
	while (low < high) {
		struct orlix_tcti_target_feature_sat_audit audit = {0};
		struct orlix_tcti_target_feature_sat_error error = {0};
		size_t middle = low + (high - low) / 2U;
		int status;

		subset.constraint_count = middle;
		status = orlix_tcti_target_feature_sat_audit(&subset, &inventory,
			ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
			ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT,
			field_domains, &audit, &error);
		orlix_tcti_target_feature_sat_audit_destroy(&audit);
		if (status && error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSAT_BASE)
			high = middle;
		else
			low = middle + 1U;
	}
	index = low - 1U;
	fprintf(stderr, "first unsatisfiable prefix=%zu root=%u offset=%zu\n",
		low, model->constraints[index],
		model->nodes[model->constraints[index]].provenance.offset);
	free(parameters);
}

struct pinned_operand_pair {
	uint32_t leaf_index;
	const char *name;
};

static int collect_pinned_operand_pairs(
	const struct orlix_tcti_target_inventory *inventory, uint32_t expression_index,
	uint32_t leaf_index, struct pinned_operand_pair **pairs, size_t *pair_count,
	size_t *pair_capacity, size_t depth)
{
	const struct orlix_tcti_target_expr *expression;
	uint32_t children[2];
	size_t child_count = 0, child_index;

	if (depth > 256U || expression_index >= inventory->expression_count)
		return -1;
	expression = &inventory->expressions[expression_index];
	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_OPERAND:
		if (!expression->text)
			return -1;
		if (*pair_count == *pair_capacity) {
			size_t next = *pair_capacity ? *pair_capacity * 2U : 64U;
			struct pinned_operand_pair *grown;

			if (next < *pair_capacity ||
				next > SIZE_MAX / sizeof(**pairs))
				return -1;
			grown = realloc(*pairs, next * sizeof(**pairs));
			if (!grown)
				return -1;
			*pairs = grown;
			*pair_capacity = next;
		}
		(*pairs)[*pair_count] = (struct pinned_operand_pair) {
			.leaf_index = leaf_index, .name = expression->text,
		};
		(*pair_count)++;
		return 0;
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		children[child_count++] = expression->left;
		break;
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN:
		children[child_count++] = expression->left;
		children[child_count++] = expression->right;
		break;
	case ORLIX_TCTI_TARGET_EXPR_SET:
		if (expression->first_item > inventory->set_item_count ||
			expression->item_count > inventory->set_item_count -
				expression->first_item)
			return -1;
		for (child_index = 0; child_index < expression->item_count;
			child_index++)
			if (collect_pinned_operand_pairs(inventory,
				inventory->set_items[expression->first_item + child_index],
				leaf_index, pairs, pair_count, pair_capacity, depth + 1U))
				return -1;
		return 0;
	default:
		return 0;
	}
	for (child_index = 0; child_index < child_count; child_index++)
		if (collect_pinned_operand_pairs(inventory, children[child_index],
			leaf_index, pairs, pair_count, pair_capacity, depth + 1U))
			return -1;
	return 0;
}

static const struct orlix_tcti_target_feature_sat_value *pinned_field_value(
	const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_feature_field_domain_bindings *field_domains,
	const struct orlix_tcti_target_feature_sat_audit *audit, size_t leaf_index,
	const char *register_name, const char *selector)
{
	const struct orlix_tcti_target_feature_sat_value *values;
	size_t value_count, value_index, binding_index;

	values = orlix_tcti_target_feature_sat_certificate_values(
		audit, leaf_index, &value_count);
	for (value_index = 0; values && value_index < value_count; value_index++) {
		if (values[value_index].kind != ORLIX_TCTI_TARGET_FEATURE_SAT_FIELD)
			continue;
		for (binding_index = 0; binding_index < field_domains->count;
			binding_index++) {
			const struct orlix_tcti_feature_field_domain_binding *binding =
				&field_domains->items[binding_index];
			const struct orlix_tcti_feature_node *node;

			if (binding->identity_group_index !=
				values[value_index].identity_group_index ||
				binding->feature_node_index >= model->node_count)
				continue;
			node = &model->nodes[binding->feature_node_index];
			if (node->kind == ORLIX_TCTI_FEATURE_FIELD &&
				node->field.register_name && node->field.selector &&
				!strcmp(node->field.register_name, register_name) &&
				!strcmp(node->field.selector, selector))
				return &values[value_index];
		}
	}
	return NULL;
}

static int pinned_integration(const char *features_path, const char *instructions_path,
	const char *registers_path)
{
	struct orlix_tcti_feature_model model = {0};
	struct orlix_tcti_target_inventory inventory = {0};
	struct orlix_tcti_register_model register_model = {0};
	struct orlix_tcti_feature_field_domain_bindings field_domains = {0};
	struct orlix_tcti_feature_error feature_error = {0};
	struct orlix_tcti_target_import_error inventory_error = {0};
	struct orlix_tcti_register_model_error register_error = {0};
	struct orlix_tcti_feature_field_domain_binding_error binding_error = {0};
	struct orlix_tcti_target_feature_sat_audit audit = {0};
	struct orlix_tcti_target_feature_sat_error sat_error = {0};
	const struct orlix_tcti_target_feature_sat_value *field_value;
	struct pinned_operand_pair *operand_pairs = NULL;
	char *features;
	char *instructions;
	char *registers;
	size_t features_length, instructions_length, registers_length;
	size_t leaf_index, row_index, value_count, pair_index, prior_index;
	size_t certificate_index, unique_condition_operands = 0;
	size_t operand_pair_count = 0, operand_pair_capacity = 0;
	size_t applicable = 0, impossible = 0, common_rows = 0, sparse_operands = 0;
	size_t raw_operand_expressions = 0;
	size_t first_impossible = SIZE_MAX;
	int status = -1;

	features = read_file(features_path, &features_length);
	instructions = read_file(instructions_path, &instructions_length);
	registers = read_file(registers_path, &registers_length);
	EXPECT(features && instructions && registers);
	EXPECT(!orlix_tcti_target_feature_model_import(features, features_length, &model,
		&feature_error));
	EXPECT(!orlix_tcti_target_inventory_import(instructions, instructions_length,
		&inventory, &inventory_error));
	EXPECT(!orlix_tcti_register_model_import(registers, registers_length,
		&register_model, &register_error));
	EXPECT(!orlix_tcti_target_feature_field_domain_bindings_build(&model,
		&register_model, &field_domains, &binding_error));
	EXPECT(field_domains.count == 605U);
	EXPECT(model.parameter_count == ORLIX_TCTI_FEATURE_PARAMETER_COUNT);
	EXPECT(model.constraint_count == ORLIX_TCTI_FEATURE_CONSTRAINT_COUNT);
	EXPECT(inventory.leaf_count == ORLIX_TCTI_A64_TARGET_LEAF_COUNT);
	for (row_index = 0; row_index < inventory.expression_count; row_index++)
		if (inventory.expressions[row_index].kind ==
			ORLIX_TCTI_TARGET_EXPR_OPERAND)
			raw_operand_expressions++;
	EXPECT(raw_operand_expressions == 663U);
	print_pinned_census(&model, &inventory);
	if (orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, &field_domains,
		&audit, &sat_error)) {
		fprintf(stderr, "pinned SAT error=%d leaf=%zu offset=%zu\n",
			sat_error.code, sat_error.leaf_index,
			sat_error.provenance.offset);
		if (sat_error.code == ORLIX_TCTI_TARGET_FEATURE_SAT_UNSAT_BASE)
			diagnose_unsat_prefix(&model, &field_domains);
		goto out;
	}
	EXPECT(audit.leaf_count == ORLIX_TCTI_A64_TARGET_LEAF_COUNT);
	for (leaf_index = 0; leaf_index < audit.leaf_count; leaf_index++) {
		const struct orlix_tcti_target_feature_sat_certificate *certificate;
		const struct orlix_tcti_target_feature_sat_value *values;

		if (audit.leaves[leaf_index] == ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE) {
			applicable++;
			certificate = orlix_tcti_target_feature_sat_certificate(
				&audit, leaf_index);
			values = orlix_tcti_target_feature_sat_certificate_values(
				&audit, leaf_index, &value_count);
			EXPECT(certificate && values &&
				certificate->condition_index ==
				inventory.leaves[leaf_index].condition);
			EXPECT(value_count >= 377U);
			for (row_index = 0; row_index < 10U; row_index++)
				EXPECT(values[row_index].kind ==
					ORLIX_TCTI_TARGET_FEATURE_SAT_CONFIGURATION);
			for (; row_index < 15U; row_index++) {
				EXPECT(values[row_index].kind ==
					ORLIX_TCTI_TARGET_FEATURE_SAT_BOOLEAN_IDENTIFIER);
				EXPECT(values[row_index].name && values[row_index].width == 1U &&
					(!strcmp(values[row_index].name, "FEAT_GICv3") ||
					 !strcmp(values[row_index].name, "FEAT_RASSAv2") ||
					 !strcmp(values[row_index].name, "FEAT_MPAM_MSC_MSMON") ||
					 !strcmp(values[row_index].name, "FEAT_MPAM_CSA") ||
					 !strcmp(values[row_index].name, "FEAT_RASSAv1p1")));
			}
			for (; row_index < 377U; row_index++)
				EXPECT(values[row_index].kind ==
					ORLIX_TCTI_TARGET_FEATURE_SAT_FIELD);
			for (; row_index < value_count; row_index++) {
				EXPECT(values[row_index].kind ==
					ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND);
				EXPECT(values[row_index].leaf_index == leaf_index);
				sparse_operands++;
			}
			common_rows += 377U;
		} else if (audit.leaves[leaf_index] ==
			 ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE) {
			if (first_impossible == SIZE_MAX)
				first_impossible = leaf_index;
			fprintf(stderr,
				"pinned impossible leaf: ordinal=%zu name=%s mnemonic=%s "
				"condition=%u condition_source=%zu+%zu\n",
				leaf_index, inventory.leaves[leaf_index].name,
				inventory.leaves[leaf_index].mnemonic,
				inventory.leaves[leaf_index].condition,
				inventory.leaves[leaf_index].condition_source_offset,
				inventory.leaves[leaf_index].condition_source_length);
			impossible++;
		} else
			EXPECT(0);
	}
	field_value = pinned_field_value(&model, &field_domains, &audit, 2235U,
		"ID_AA64MMFR4_EL1", "TEV");
	EXPECT(field_value && field_value->low == 1U && field_value->high == 0U);
	field_value = pinned_field_value(&model, &field_domains, &audit, 2308U,
		"ID_AA64MMFR3_EL1", "S1POE");
	EXPECT(field_value && field_value->low == 2U && field_value->high == 0U);
	field_value = pinned_field_value(&model, &field_domains, &audit, 2308U,
		"ID_AA64MMFR3_EL1", "S1PIE");
	EXPECT(field_value && field_value->low == 1U && field_value->high == 0U);
	field_value = pinned_field_value(&model, &field_domains, &audit, 2675U,
		"ID_AA64ISAR2_EL1", "MOPS");
	EXPECT(field_value && field_value->low == 2U && field_value->high == 0U);
	field_value = pinned_field_value(&model, &field_domains, &audit, 2675U,
		"ID_AA64PFR1_EL1", "MTE");
	EXPECT(field_value && field_value->low == 1U && field_value->high == 0U);
	for (leaf_index = 0; leaf_index < inventory.leaf_count; leaf_index++)
		EXPECT(!collect_pinned_operand_pairs(&inventory,
			inventory.leaves[leaf_index].condition, (uint32_t)leaf_index,
			&operand_pairs, &operand_pair_count, &operand_pair_capacity, 0));
	EXPECT(operand_pair_count == 423U);
	for (pair_index = 0; pair_index < operand_pair_count; pair_index++) {
		const struct pinned_operand_pair *operand = &operand_pairs[pair_index];
		size_t matches = 0;

		for (prior_index = 0; prior_index < pair_index; prior_index++) {
			const struct pinned_operand_pair *prior = &operand_pairs[prior_index];

			if (prior->leaf_index == operand->leaf_index &&
				!strcmp(prior->name, operand->name))
				break;
		}
		if (prior_index != pair_index)
			continue;
		unique_condition_operands++;
		for (certificate_index = 0;
			certificate_index < audit.certificate_value_count;
			certificate_index++) {
			const struct orlix_tcti_target_feature_sat_value *value =
				&audit.certificate_values[certificate_index];

			if (value->kind == ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND &&
				value->leaf_index == operand->leaf_index && value->name &&
				!strcmp(value->name, operand->name))
				matches++;
		}
		if (matches != 1U)
			fprintf(stderr,
				"pinned reachable operand mismatch: pair=%zu leaf=%u name=%s matches=%zu\n",
				pair_index, operand->leaf_index, operand->name, matches);
		EXPECT(matches == 1U);
	}
	for (certificate_index = 0;
		certificate_index < audit.certificate_value_count;
		certificate_index++) {
		const struct orlix_tcti_target_feature_sat_value *value =
			&audit.certificate_values[certificate_index];
		size_t matches = 0;

		if (value->kind != ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND)
			continue;
		for (pair_index = 0; pair_index < operand_pair_count; pair_index++) {
			const struct pinned_operand_pair *operand = &operand_pairs[pair_index];

			if (operand->leaf_index == value->leaf_index && value->name &&
				!strcmp(operand->name, value->name))
				matches++;
		}
		EXPECT(matches > 0U);
	}
	if (unique_condition_operands != 364U)
		fprintf(stderr,
			"pinned operand decomposition: expressions=%zu pairs=%zu unique=%zu certificates=%zu\n",
			raw_operand_expressions, operand_pair_count,
			unique_condition_operands, sparse_operands);
	EXPECT(unique_condition_operands == 364U);
	if (common_rows != 1639950U || sparse_operands != 364U)
		fprintf(stderr,
			"pinned certificate drift: applicable=%zu impossible=%zu "
			"first_impossible=%zu common=%zu operands=%zu total=%zu\n",
			applicable, impossible, first_impossible, common_rows,
			sparse_operands, audit.certificate_value_count);
	EXPECT(applicable + impossible == ORLIX_TCTI_A64_TARGET_LEAF_COUNT);
	EXPECT(common_rows == 1639950U);
	EXPECT(sparse_operands == 364U);
	EXPECT(audit.certificate_value_count == common_rows + sparse_operands);
	fprintf(stdout,
		"pinned feature applicability: evaluated=%zu unresolved=0 applicable=%zu impossible=%zu\n",
		audit.leaf_count, applicable, impossible);
	status = 0;
out:
	free(operand_pairs);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	orlix_tcti_target_feature_field_domain_bindings_destroy(&field_domains);
	orlix_tcti_register_model_destroy(&register_model);
	orlix_tcti_target_inventory_destroy(&inventory);
	orlix_tcti_target_feature_model_destroy(&model);
	free(registers);
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
	EXPECT(!accepts_unused_supported_numeric_nodes());
	EXPECT(!exact_numeric_constants());
	EXPECT(!inactive_field_domain_guard());
	EXPECT(!finite_field_domain());
	EXPECT(!exact_guarded_range_and_constraint_domains());
	EXPECT(!accepts_unused_supported_target_scalar());
	EXPECT(!exact_configuration_certificates());
	EXPECT(!exact_target_operand_patterns());
	EXPECT(!rejects_direct_input_cycles());
	EXPECT(!rejects_literal_namespace_over_int_max());
	EXPECT(!cumulative_resource_limits());
	if (argc == 4)
		EXPECT(!pinned_integration(argv[1], argv[2], argv[3]));
	else
		EXPECT(argc == 1);
	puts("target feature SAT tests: passed");
	return 0;
}
