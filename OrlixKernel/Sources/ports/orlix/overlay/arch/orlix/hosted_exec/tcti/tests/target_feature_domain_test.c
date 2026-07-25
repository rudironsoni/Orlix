/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_domain.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

enum {
	N_TRUE,
	N_FALSE,
	N_FEATURE,
	N_FOUR,
	N_NEGATIVE,
	N_A,
	N_B,
	N_DOT_A_B,
	N_A_DOT_B,
	N_SET,
	N_UINT_FOUR,
	N_SINT_FOUR,
	N_FIELD,
	N_NOT,
	N_AND,
	N_OR,
	N_EQ_DOT,
	N_NE_ATOM,
	N_FIVE,
	N_LT,
	N_GT,
	N_GE,
	N_IN,
	N_IMPLIES,
	N_IFF,
	N_UINT_NEGATIVE,
	N_FIELD_EQ,
	N_COUNT,
};

static tcti_feature_artifact_u32 children[] = {
	N_A, N_B,		/* N_DOT_A_B */
	N_A, N_A_DOT_B,	/* N_SET */
	N_FOUR,		/* N_UINT_FOUR */
	N_UINT_FOUR,	/* N_SINT_FOUR */
	N_NEGATIVE,	/* N_UINT_NEGATIVE */
};

static struct tcti_feature_artifact_node nodes[N_COUNT] = {
	[N_TRUE] = { .kind = TCTI_FEATURE_ARTIFACT_BOOL, .integer = 1,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_FALSE] = { .kind = TCTI_FEATURE_ARTIFACT_BOOL,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_FEATURE] = { .kind = TCTI_FEATURE_ARTIFACT_IDENTIFIER, .text = "F",
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_FOUR] = { .kind = TCTI_FEATURE_ARTIFACT_INTEGER, .integer = 4,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_NEGATIVE] = { .kind = TCTI_FEATURE_ARTIFACT_INTEGER, .integer = -1,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_A] = { .kind = TCTI_FEATURE_ARTIFACT_VALUE, .text = "A",
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_B] = { .kind = TCTI_FEATURE_ARTIFACT_VALUE, .text = "B",
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_DOT_A_B] = { .kind = TCTI_FEATURE_ARTIFACT_DOT_ATOM,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = 0, .child_count = 2 },
	[N_A_DOT_B] = { .kind = TCTI_FEATURE_ARTIFACT_VALUE, .text = "A.B",
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_SET] = { .kind = TCTI_FEATURE_ARTIFACT_SET,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = 2, .child_count = 2 },
	[N_UINT_FOUR] = { .kind = TCTI_FEATURE_ARTIFACT_UINT,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = 4, .child_count = 1 },
	[N_SINT_FOUR] = { .kind = TCTI_FEATURE_ARTIFACT_SINT,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = 5, .child_count = 1 },
	[N_FIELD] = { .kind = TCTI_FEATURE_ARTIFACT_FIELD,
		.field_state = "Current", .field_register_name = "ID_TEST",
		.field_selector = "Value",
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_NOT] = { .kind = TCTI_FEATURE_ARTIFACT_NOT, .left = N_FALSE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_AND] = { .kind = TCTI_FEATURE_ARTIFACT_AND, .left = N_TRUE,
		.right = N_FEATURE, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_OR] = { .kind = TCTI_FEATURE_ARTIFACT_OR, .left = N_FALSE,
		.right = N_FEATURE, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_EQ_DOT] = { .kind = TCTI_FEATURE_ARTIFACT_EQ, .left = N_DOT_A_B,
		.right = N_A_DOT_B, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_NE_ATOM] = { .kind = TCTI_FEATURE_ARTIFACT_NE, .left = N_A,
		.right = N_B, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_FIVE] = { .kind = TCTI_FEATURE_ARTIFACT_INTEGER, .integer = 5,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_LT] = { .kind = TCTI_FEATURE_ARTIFACT_LT, .left = N_FOUR,
		.right = N_FIVE, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_GT] = { .kind = TCTI_FEATURE_ARTIFACT_GT, .left = N_FIVE,
		.right = N_FOUR, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_GE] = { .kind = TCTI_FEATURE_ARTIFACT_GE, .left = N_FIVE,
		.right = N_FIVE, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_IN] = { .kind = TCTI_FEATURE_ARTIFACT_IN, .left = N_DOT_A_B,
		.right = N_SET, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_IMPLIES] = { .kind = TCTI_FEATURE_ARTIFACT_IMPLIES, .left = N_TRUE,
		.right = N_FEATURE, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_IFF] = { .kind = TCTI_FEATURE_ARTIFACT_IFF, .left = N_FEATURE,
		.right = N_TRUE, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_UINT_NEGATIVE] = { .kind = TCTI_FEATURE_ARTIFACT_UINT,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = 6, .child_count = 1 },
	[N_FIELD_EQ] = { .kind = TCTI_FEATURE_ARTIFACT_EQ, .left = N_FIELD,
		.right = N_FIVE, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
};

static const struct tcti_feature_artifact_constraint constraints[] = {
	{ .node_index = N_AND }, { .node_index = N_OR },
	{ .node_index = N_EQ_DOT }, { .node_index = N_NE_ATOM },
	{ .node_index = N_LT }, { .node_index = N_GT },
	{ .node_index = N_GE }, { .node_index = N_IN },
	{ .node_index = N_IMPLIES }, { .node_index = N_IFF },
	{ .node_index = N_FIELD_EQ },
};

static const struct tcti_feature_artifact fixture = {
	.counts = { .constraint_count = sizeof(constraints) / sizeof(constraints[0]),
		.node_count = N_COUNT, .child_count = sizeof(children) / sizeof(children[0]) },
	.constraints = constraints,
	.nodes = nodes,
	.children = children,
};

static tcti_feature_artifact_u8 active[N_COUNT];

static int feature(void *context, const char *name,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	if (strcmp(name, "F"))
		return -1;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL, .boolean = 1,
	};
	return 0;
}

static int field(void *context, const char *state, const char *register_name,
	const char *selector, struct tcti_feature_domain_value *value)
{
	(void)context;
	if (strcmp(state, "Current") || strcmp(register_name, "ID_TEST") ||
	    strcmp(selector, "Value"))
		return -1;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_SIGNED, .signed_value = 5,
	};
	return 0;
}

static int malformed_feature_value(void *context, const char *name,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	(void)name;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED, .unsigned_value = 1,
	};
	return 0;
}

static const struct tcti_feature_domain_environment environment = {
	.feature = feature,
	.field = field,
};

static struct tcti_feature_domain_scratch scratch = {
	.active = active,
	.active_count = sizeof(active),
	.max_depth = 64,
};

static int tcnd_feature_terminals_bind_to_checked_parameters(void)
{
	static const struct tcti_feature_artifact_parameter parameters[] = {
		{ .name = "F" },
	};
	static const struct tcti_feature_artifact artifact = {
		.counts = { .parameter_count = 1 },
		.parameters = parameters,
	};
	struct tcti_feature_domain_tcnd_diagnostic diagnostic;

	CHECK(!tcti_feature_domain_validate_tcnd_features(&artifact,
		"54434e440102000000050000000146", &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_TCND_OK);
	CHECK(diagnostic.feature_terminals == 1);
	CHECK(tcti_feature_domain_validate_tcnd_features(&artifact,
		"54434e440102000000050000000158", &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_TCND_UNKNOWN_FEATURE);
	CHECK(tcti_feature_domain_validate_tcnd_features(&artifact,
		"54434e4401020000000500000001", &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_TCND_ENCODING);
	return 0;
}

static int evaluates_all_pinned_node_representations(void)
{
	struct tcti_feature_domain_value value;
	struct tcti_feature_domain_diagnostic diagnostic;
	tcti_feature_artifact_u8 satisfied;

	CHECK(!tcti_feature_domain_evaluate(&fixture, N_UINT_FOUR, &environment,
					    &scratch, &value, &diagnostic));
	CHECK(value.kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED);
	CHECK(value.unsigned_value == 4);
	CHECK(!tcti_feature_domain_evaluate(&fixture, N_SINT_FOUR, &environment,
					    &scratch, &value, &diagnostic));
	CHECK(value.kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED);
	CHECK(value.signed_value == 4);
	CHECK(!tcti_feature_domain_evaluate_constraints(&fixture, &environment,
						&scratch, &satisfied, &diagnostic));
	CHECK(satisfied == 1);
	return 0;
}

static int fails_loud_on_missing_values_types_and_overflow(void)
{
	struct tcti_feature_domain_value value;
	struct tcti_feature_domain_diagnostic diagnostic;
	struct tcti_feature_domain_environment missing_feature = environment;
	struct tcti_feature_domain_environment missing_field = environment;
	struct tcti_feature_domain_environment invalid_callback = environment;

	missing_feature.feature = NULL;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_FEATURE, &missing_feature,
					   &scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_MISSING_FEATURE);
	missing_field.field = NULL;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_FIELD, &missing_field,
					   &scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_MISSING_FIELD);
	invalid_callback.feature = malformed_feature_value;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_FEATURE, &invalid_callback,
					   &scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_CALLBACK_VALUE);
	CHECK(tcti_feature_domain_evaluate(&fixture, N_UINT_NEGATIVE, &environment,
					   &scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_OVERFLOW);
	nodes[N_AND].left = N_FOUR;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_AND, &environment, &scratch,
					   &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_TYPE);
	nodes[N_AND].left = N_TRUE;
	return 0;
}

static int fails_loud_on_malformed_references_and_cycles(void)
{
	struct tcti_feature_domain_value value;
	struct tcti_feature_domain_diagnostic diagnostic;
	tcti_feature_artifact_u32 original = nodes[N_NOT].left;
	tcti_feature_artifact_u32 set_child = children[2];

	nodes[N_NOT].left = N_COUNT;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_NOT, &environment, &scratch,
					   &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_REFERENCE);
	nodes[N_NOT].left = original;
	nodes[N_NOT].left = N_NOT;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_NOT, &environment, &scratch,
					   &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_CYCLE);
	nodes[N_NOT].left = original;
	children[2] = N_COUNT;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_IN, &environment, &scratch,
					   &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_REFERENCE);
	children[2] = set_child;
	return 0;
}

static int rejects_insufficient_scratch_and_depth(void)
{
	struct tcti_feature_domain_value value;
	struct tcti_feature_domain_diagnostic diagnostic;
	struct tcti_feature_domain_scratch short_scratch = scratch;

	short_scratch.active_count--;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_TRUE, &environment,
					   &short_scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_INVALID_ARGUMENT);
	short_scratch = scratch;
	short_scratch.max_depth = 1;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_NOT, &environment,
					   &short_scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_DEPTH);
	return 0;
}

int main(void)
{
	if (tcnd_feature_terminals_bind_to_checked_parameters() ||
	    evaluates_all_pinned_node_representations() ||
	    fails_loud_on_missing_values_types_and_overflow() ||
	    fails_loud_on_malformed_references_and_cycles() ||
	    rejects_insufficient_scratch_and_depth())
		return 1;
	puts("PASS target feature-domain evaluator");
	return 0;
}
