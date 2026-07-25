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
	N_FIELD_GE,
	N_BINARY_VALUE,
	N_UINT_BINARY_VALUE,
	N_SINT_BINARY_VALUE,
	N_COUNT,
};

static tcti_feature_artifact_u32 children[] = {
	N_A, N_B,		/* N_DOT_A_B */
	N_A, N_A_DOT_B,	/* N_SET */
	N_FOUR,		/* N_UINT_FOUR */
	N_UINT_FOUR,	/* N_SINT_FOUR */
	N_NEGATIVE,	/* N_UINT_NEGATIVE */
	N_BINARY_VALUE,	/* N_UINT_BINARY_VALUE */
	N_BINARY_VALUE,	/* N_SINT_BINARY_VALUE */
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
	[N_FIELD_GE] = { .kind = TCTI_FEATURE_ARTIFACT_GE, .left = N_FIELD,
		.right = N_FOUR, .first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_BINARY_VALUE] = { .kind = TCTI_FEATURE_ARTIFACT_VALUE,
		.text = "'10000000'", .left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE },
	[N_UINT_BINARY_VALUE] = { .kind = TCTI_FEATURE_ARTIFACT_UINT,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = 7, .child_count = 1 },
	[N_SINT_BINARY_VALUE] = { .kind = TCTI_FEATURE_ARTIFACT_SINT,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = 8, .child_count = 1 },
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
		.kind = TCTI_FEATURE_DOMAIN_VALUE_SIGNED,
		.integer = { .low = 5, .width = 4U },
	};
	return 0;
}

static int field_low_overflow(void *context, const char *state,
	const char *register_name, const char *selector,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	(void)state;
	(void)register_name;
	(void)selector;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED,
		.integer = { .low = 2, .width = 1U },
	};
	return 0;
}

static int field_high_overflow(void *context, const char *state,
	const char *register_name, const char *selector,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	(void)state;
	(void)register_name;
	(void)selector;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED,
		.integer = { .high = 2, .width = 65U },
	};
	return 0;
}

static int malformed_feature_value(void *context, const char *name,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	(void)name;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED,
		.integer = { .low = 1, .width = 0U },
	};
	return 0;
}

static int malformed_feature_boolean(void *context, const char *name,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	(void)name;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL, .boolean = 2U,
	};
	return 0;
}

static int malformed_field_boolean(void *context, const char *state,
	const char *register_name, const char *selector,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	(void)state;
	(void)register_name;
	(void)selector;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL, .boolean = 2U,
	};
	return 0;
}

static const struct tcti_feature_domain_environment environment = {
	.feature = feature,
	.field = field,
};

static int disabled_feature(void *context, const char *name,
	struct tcti_feature_domain_value *value)
{
	(void)context;
	if (strcmp(name, "F"))
		return -1;
	*value = (struct tcti_feature_domain_value) {
		.kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL,
		.boolean = 0,
	};
	return 0;
}

static const struct tcti_feature_domain_environment disabled_environment = {
	.feature = disabled_feature,
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
	CHECK(value.integer.low == 4 && value.integer.high == 0 &&
	      value.integer.width == 64U);
	CHECK(!tcti_feature_domain_evaluate(&fixture, N_UINT_BINARY_VALUE,
					    &environment, &scratch, &value,
					    &diagnostic));
	CHECK(value.kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED &&
	      value.integer.low == 128U && value.integer.width == 8U);
	CHECK(!tcti_feature_domain_evaluate(&fixture, N_SINT_BINARY_VALUE,
					    &environment, &scratch, &value,
					    &diagnostic));
	CHECK(value.kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED &&
	      value.integer.low == 128U && value.integer.width == 8U);
	CHECK(!tcti_feature_domain_evaluate(&fixture, N_SINT_FOUR, &environment,
					    &scratch, &value, &diagnostic));
	CHECK(value.kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED);
	CHECK(value.integer.low == 4 && value.integer.high == 0 &&
	      value.integer.width == 64U);
	CHECK(!tcti_feature_domain_evaluate(&fixture, N_FIELD_GE, &environment,
					    &scratch, &value, &diagnostic));
	CHECK(value.kind == TCTI_FEATURE_DOMAIN_VALUE_BOOL && value.boolean);
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
	struct tcti_feature_domain_environment invalid_low_field = environment;
	struct tcti_feature_domain_environment invalid_high_field = environment;
	struct tcti_feature_domain_environment invalid_boolean_feature = environment;
	struct tcti_feature_domain_environment invalid_boolean_field = environment;

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
	invalid_boolean_feature.feature = malformed_feature_boolean;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_FEATURE,
					   &invalid_boolean_feature, &scratch,
					   &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_CALLBACK_VALUE);
	invalid_low_field.field = field_low_overflow;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_FIELD, &invalid_low_field,
					   &scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_CALLBACK_VALUE);
	invalid_high_field.field = field_high_overflow;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_FIELD, &invalid_high_field,
					   &scratch, &value, &diagnostic));
	CHECK(diagnostic.error == TCTI_FEATURE_DOMAIN_CALLBACK_VALUE);
	invalid_boolean_field.field = malformed_field_boolean;
	CHECK(tcti_feature_domain_evaluate(&fixture, N_FIELD,
					   &invalid_boolean_field, &scratch,
					   &value, &diagnostic));
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

static int parses_and_compares_exact_128_bit_value_literals(void)
{
	static const char high_bit_128[] =
		"'"
		"1000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000" "'";
	static const char zero_128[] =
		"'"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000" "'";
	static const char overwide[] =
		"'"
		"1000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "00000000000000000" "'";
	static const char signed_64_min[] =
		"'"
		"1000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000" "'";
	static const char signed_128_extended[] =
		"'"
		"1111111111111111" "1111111111111111"
		"1111111111111111" "1111111111111111"
		"1000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000" "'";
	static const char uint_65_high_bit[] =
		"'"
		"1000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000" "0'";
	static const char uint_127_high_bit[] =
		"'"
		"1000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "000000000000000'";
	static const char signed_65_min[] =
		"'"
		"1000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000" "0'";
	static const char signed_65_extended[] =
		"'"
		"1111111111111111" "1111111111111111"
		"1111111111111111" "1111111111111111"
		"0000000000000000" "0000000000000000"
		"0000000000000000" "0000000000000000" "'";
	struct tcti_feature_domain_value high;
	struct tcti_feature_domain_value zero;
	struct tcti_feature_domain_value eight_bit;
	struct tcti_feature_domain_value signed_short;
	struct tcti_feature_domain_value signed_extended;
	struct tcti_feature_domain_value unsigned_short;
	struct tcti_feature_domain_value unsigned_extended;
	struct tcti_feature_domain_value signed_64;
	struct tcti_feature_domain_value signed_128;
	struct tcti_feature_domain_value boundary;
	struct tcti_feature_domain_value signed_65;
	struct tcti_feature_domain_value signed_65_wide;
	int order;

	CHECK(!tcti_feature_domain_parse_uint_literal(high_bit_128, &high));
	CHECK(high.kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED);
	CHECK(high.integer.width == 128U && high.integer.low == 0 &&
	      high.integer.high == (tcti_feature_artifact_u64)1U << 63);
	CHECK(!tcti_feature_domain_parse_sint_literal(high_bit_128, &high));
	CHECK(!tcti_feature_domain_parse_sint_literal(zero_128, &zero));
	CHECK(!tcti_feature_domain_compare_numeric(&high, &zero, &order));
	CHECK(order < 0);
	CHECK(!tcti_feature_domain_parse_uint_literal("'11111111'", &eight_bit));
	CHECK(tcti_feature_domain_compare_numeric(&high, &eight_bit, &order));
	CHECK(!tcti_feature_domain_parse_uint_literal("'1'", &boundary));
	CHECK(boundary.integer.width == 1U && boundary.integer.low == 1U &&
	      boundary.integer.high == 0);
	CHECK(!tcti_feature_domain_parse_uint_literal(signed_64_min, &boundary));
	CHECK(boundary.integer.width == 64U &&
	      boundary.integer.low == (tcti_feature_artifact_u64)1U << 63 &&
	      boundary.integer.high == 0);
	CHECK(!tcti_feature_domain_parse_uint_literal(uint_65_high_bit, &boundary));
	CHECK(boundary.integer.width == 65U && boundary.integer.low == 0 &&
	      boundary.integer.high == 1U);
	CHECK(!tcti_feature_domain_parse_uint_literal(uint_127_high_bit, &boundary));
	CHECK(boundary.integer.width == 127U && boundary.integer.low == 0 &&
	      boundary.integer.high == (tcti_feature_artifact_u64)1U << 62);
	CHECK(!tcti_feature_domain_parse_sint_literal("'10'", &signed_short));
	CHECK(!tcti_feature_domain_parse_sint_literal("'11111110'", &signed_extended));
	CHECK(!tcti_feature_domain_compare_numeric(&signed_short,
						       &signed_extended, &order));
	CHECK(order == 0);
	CHECK(!tcti_feature_domain_parse_uint_literal("'10'", &unsigned_short));
	CHECK(!tcti_feature_domain_parse_uint_literal("'00000010'",
						       &unsigned_extended));
	CHECK(!tcti_feature_domain_compare_numeric(&unsigned_short,
						       &unsigned_extended, &order));
	CHECK(order == 0);
	CHECK(!tcti_feature_domain_parse_sint_literal(signed_64_min, &signed_64));
	CHECK(!tcti_feature_domain_parse_sint_literal(signed_128_extended,
						       &signed_128));
	CHECK(!tcti_feature_domain_compare_numeric(&signed_64, &signed_128,
						       &order));
	CHECK(order == 0);
	CHECK(!tcti_feature_domain_parse_sint_literal(signed_65_min, &signed_65));
	CHECK(!tcti_feature_domain_parse_sint_literal(signed_65_extended,
						       &signed_65_wide));
	CHECK(!tcti_feature_domain_compare_numeric(&signed_65, &signed_65_wide,
						       &order));
	CHECK(order == 0);
	CHECK(tcti_feature_domain_parse_uint_literal("'" "'", &zero));
	CHECK(tcti_feature_domain_parse_uint_literal("'102'", &zero));
	CHECK(tcti_feature_domain_parse_uint_literal(overwide, &zero));
	CHECK(tcti_feature_domain_parse_uint_literal("111", &zero));
	return 0;
}

static int feature_configuration_union_retains_all_candidates(void)
{
	static const struct tcti_feature_artifact_parameter parameters[] = {
		{ .name = "F" },
	};
	static const struct tcti_feature_artifact_constraint constraints[] = {
		{ .node_index = N_FEATURE },
	};
	static const struct tcti_feature_artifact artifact = {
		.counts = { .parameter_count = 1, .constraint_count = 1,
			.node_count = N_COUNT },
		.parameters = parameters,
		.constraints = constraints,
		.nodes = nodes,
		.children = children,
	};
	struct tcti_feature_domain_union_candidate candidates[] = {
		{ .environment = &disabled_environment },
		{ .environment = &environment },
	};
	tcti_feature_artifact_u8 active_a[N_COUNT];
	tcti_feature_artifact_u8 active_b[N_COUNT];
	struct tcti_feature_domain_scratch evaluators[] = {
		{ .active = active_a, .active_count = N_COUNT, .max_depth = 64 },
		{ .active = active_b, .active_count = N_COUNT, .max_depth = 64 },
	};
	struct tcti_feature_domain_union_scratch union_scratch = {
		.evaluators = evaluators,
		.evaluator_count = 2,
	};
	struct tcti_feature_domain_union_result result;

	CHECK(!tcti_feature_domain_evaluate_constraint_union(&artifact,
		candidates, 2, &union_scratch, &result));
	CHECK(result.candidate_count == 2);
	CHECK(result.evaluated_count == 2);
	CHECK(result.satisfied_count == 1);
	CHECK(result.unsatisfied_count == 1);
	CHECK(result.unsupported_count == 0);
	return 0;
}

static int feature_configuration_union_reports_unsupported_candidates(void)
{
	static const struct tcti_feature_artifact_parameter parameters[] = {
		{ .name = "F" },
	};
	static const struct tcti_feature_artifact_constraint constraints[] = {
		{ .node_index = N_FEATURE },
	};
	static const struct tcti_feature_artifact artifact = {
		.counts = { .parameter_count = 1, .constraint_count = 1,
			.node_count = N_COUNT },
		.parameters = parameters,
		.constraints = constraints,
		.nodes = nodes,
		.children = children,
	};
	struct tcti_feature_domain_environment missing = environment;
	struct tcti_feature_domain_union_candidate candidates[] = {
		{ .environment = &environment },
		{ .environment = &missing },
	};
	tcti_feature_artifact_u8 active_a[N_COUNT];
	tcti_feature_artifact_u8 active_b[N_COUNT];
	struct tcti_feature_domain_scratch evaluators[] = {
		{ .active = active_a, .active_count = N_COUNT, .max_depth = 64 },
		{ .active = active_b, .active_count = N_COUNT, .max_depth = 64 },
	};
	struct tcti_feature_domain_union_scratch union_scratch = {
		.evaluators = evaluators,
		.evaluator_count = 2,
	};
	struct tcti_feature_domain_union_result result;

	missing.feature = NULL;
	CHECK(tcti_feature_domain_evaluate_constraint_union(&artifact,
		candidates, 2, &union_scratch, &result));
	CHECK(result.evaluated_count == 1);
	CHECK(result.satisfied_count == 1);
	CHECK(result.unsupported_count == 1);
	CHECK(result.first_unsupported.error ==
		TCTI_FEATURE_DOMAIN_MISSING_FEATURE);
	CHECK(tcti_feature_domain_evaluate_constraint_union(&artifact,
		candidates, 0, &union_scratch, &result));
	return 0;
}

struct tcnd_fixture_context {
	tcti_feature_artifact_u8 feature;
	const char *operand;
};

static int tcnd_hex_nibble(char byte, unsigned int *value)
{
	if (byte >= '0' && byte <= '9') {
		*value = (unsigned int)(byte - '0');
		return 0;
	}
	if (byte >= 'a' && byte <= 'f') {
		*value = (unsigned int)(byte - 'a' + 10);
		return 0;
	}
	if (byte >= 'A' && byte <= 'F') {
		*value = (unsigned int)(byte - 'A' + 10);
		return 0;
	}
	return -1;
}

static int tcnd_source_equals(const char *hex, size_t offset, size_t length,
	const char *text)
{
	size_t index;

	if (strlen(text) != length)
		return 0;
	for (index = 0; index < length; index++) {
		unsigned int high;
		unsigned int low;

		if (tcnd_hex_nibble(hex[(offset + index) * 2U], &high) ||
		    tcnd_hex_nibble(hex[(offset + index) * 2U + 1U], &low) ||
		    (unsigned char)((high << 4) | low) != (unsigned char)text[index])
			return 0;
	}
	return 1;
}

static int tcnd_feature(void *context, const char *hex, size_t offset,
	size_t length, tcti_feature_artifact_u8 *enabled)
{
	struct tcnd_fixture_context *fixture = context;

	if (!tcnd_source_equals(hex, offset, length, "F"))
		return -1;
	*enabled = fixture->feature;
	return 0;
}

static int tcnd_operand(void *context, const char *hex, size_t offset,
	size_t length, const char **value, size_t *value_length)
{
	struct tcnd_fixture_context *fixture = context;

	if (!tcnd_source_equals(hex, offset, length, "op"))
		return -1;
	*value = fixture->operand;
	*value_length = strlen(*value);
	return 0;
}

static int tcnd_union_evaluates_features_and_operand_alternatives(void)
{
	/* AND(FEATURE(F), EQ(OPERAND(op), VALUE(A))). */
	static const char condition[] =
		"54434e4401070000002402000000050000000146"
		"09000000150300000006000000026f7004000000050000000141";
	static const struct tcti_feature_artifact_parameter parameters[] = {
		{ .name = "F" },
	};
	static const struct tcti_feature_artifact artifact = {
		.counts = { .parameter_count = 1 },
		.parameters = parameters,
	};
	struct tcnd_fixture_context first = { .feature = 0, .operand = "A" };
	struct tcnd_fixture_context second = { .feature = 1, .operand = "A" };
	struct tcnd_fixture_context third = { .feature = 1, .operand = "B" };
	struct tcti_feature_domain_tcnd_environment environments[] = {
		{ .context = &first, .feature = tcnd_feature, .operand = tcnd_operand },
		{ .context = &second, .feature = tcnd_feature, .operand = tcnd_operand },
		{ .context = &third, .feature = tcnd_feature, .operand = tcnd_operand },
	};
	struct tcti_feature_domain_tcnd_union_candidate candidates[] = {
		{ .environment = &environments[0] },
		{ .environment = &environments[1] },
		{ .environment = &environments[2] },
	};
	struct tcti_feature_domain_tcnd_union_result result;
	struct tcti_feature_domain_tcnd_diagnostic diagnostic;
	tcti_feature_artifact_u8 satisfied;

	CHECK(!tcti_feature_domain_evaluate_tcnd(&artifact, condition,
		&environments[1], &satisfied, &diagnostic));
	CHECK(satisfied == 1);
	CHECK(!tcti_feature_domain_evaluate_tcnd_union(&artifact, condition,
		candidates, 3, &result));
	CHECK(result.candidate_count == 3);
	CHECK(result.evaluated_count == 3);
	CHECK(result.satisfied_count == 1);
	CHECK(result.unsatisfied_count == 2);
	CHECK(result.unsupported_count == 0);
	return 0;
}

static int tcnd_union_fails_loudly_on_unbound_operands(void)
{
	static const char condition[] =
		"54434e440109000000150300000006000000026f7004000000050000000141";
	static const struct tcti_feature_artifact_parameter parameters[] = {
		{ .name = "unused" },
	};
	static const struct tcti_feature_artifact artifact = {
		.parameters = parameters,
	};
	struct tcnd_fixture_context fixture = { .feature = 1, .operand = "A" };
	struct tcti_feature_domain_tcnd_environment environment = {
		.context = &fixture,
		.feature = tcnd_feature,
	};
	struct tcti_feature_domain_tcnd_union_candidate candidate = {
		.environment = &environment,
	};
	struct tcti_feature_domain_tcnd_union_result result;

	CHECK(tcti_feature_domain_evaluate_tcnd_union(&artifact, condition,
		&candidate, 1, &result));
	CHECK(result.evaluated_count == 0);
	CHECK(result.unsupported_count == 1);
	CHECK(result.first_unsupported.error ==
		TCTI_FEATURE_DOMAIN_TCND_MISSING_OPERAND);
	return 0;
}

static int tcnd_union_evaluates_not_or_and_set_membership(void)
{
	/* OR(NOT(FEATURE(F)), IN(OPERAND(op), SET(VALUE(A), VALUE(B)))). */
	static const char condition[] =
		"54434e4401080000003c060000000a02000000050000000146"
		"0b000000280300000006000000026f70050000001800000002"
		"0400000005000000014104000000050000000142";
	static const struct tcti_feature_artifact_parameter parameters[] = {
		{ .name = "F" },
	};
	static const struct tcti_feature_artifact artifact = {
		.counts = { .parameter_count = 1 },
		.parameters = parameters,
	};
	struct tcnd_fixture_context first = { .feature = 1, .operand = "B" };
	struct tcnd_fixture_context second = { .feature = 1, .operand = "C" };
	struct tcnd_fixture_context third = { .feature = 0, .operand = "C" };
	struct tcti_feature_domain_tcnd_environment environments[] = {
		{ .context = &first, .feature = tcnd_feature, .operand = tcnd_operand },
		{ .context = &second, .feature = tcnd_feature, .operand = tcnd_operand },
		{ .context = &third, .feature = tcnd_feature, .operand = tcnd_operand },
	};
	struct tcti_feature_domain_tcnd_union_candidate candidates[] = {
		{ .environment = &environments[0] },
		{ .environment = &environments[1] },
		{ .environment = &environments[2] },
	};
	struct tcti_feature_domain_tcnd_union_result result;

	CHECK(!tcti_feature_domain_evaluate_tcnd_union(&artifact, condition,
		candidates, 3, &result));
	CHECK(result.evaluated_count == 3);
	CHECK(result.satisfied_count == 2);
	CHECK(result.unsatisfied_count == 1);
	return 0;
}

static int source_ordinals_900_through_1199_are_explicitly_evaluated(void)
{
	const struct tcti_feature_artifact *artifact =
		tcti_feature_artifact_canonical();
	static const struct tcti_feature_domain_tcnd_environment no_assignment;
	static const struct tcti_feature_domain_tcnd_union_candidate candidate = {
		.environment = &no_assignment,
	};
	size_t total = 0;
	size_t evaluated = 0;
	size_t satisfied = 0;
	size_t missing_feature = 0;
	size_t missing_operand = 0;
	size_t invalid = 0;

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...) \
	do { } while (0);
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, condition, source_offset, source_length) \
	do { \
		if ((ordinal) >= 900U && (ordinal) < 1200U) { \
			struct tcti_feature_domain_tcnd_union_result result; \
			total++; \
			if (!tcti_feature_domain_evaluate_tcnd_union(artifact, \
				condition, &candidate, 1, &result)) { \
				evaluated += result.evaluated_count; \
				satisfied += result.satisfied_count; \
			} else if (result.first_unsupported.error == \
				TCTI_FEATURE_DOMAIN_TCND_MISSING_FEATURE) { \
				missing_feature++; \
			} else if (result.first_unsupported.error == \
				TCTI_FEATURE_DOMAIN_TCND_MISSING_OPERAND) { \
				missing_operand++; \
			} else { \
				invalid++; \
			} \
		} \
	} while (0);
#include "../isa/source_manifest.def"
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

	CHECK(total == 300);
	CHECK(evaluated == 0);
	CHECK(satisfied == 0);
	CHECK(missing_feature == 248);
	CHECK(missing_operand == 52);
	CHECK(invalid == 0);
	return 0;
}

int main(void)
{
	if (tcnd_feature_terminals_bind_to_checked_parameters() ||
	    evaluates_all_pinned_node_representations() ||
	    fails_loud_on_missing_values_types_and_overflow() ||
	    fails_loud_on_malformed_references_and_cycles() ||
	    rejects_insufficient_scratch_and_depth() ||
	    parses_and_compares_exact_128_bit_value_literals() ||
	    feature_configuration_union_retains_all_candidates() ||
	    feature_configuration_union_reports_unsupported_candidates() ||
	    tcnd_union_evaluates_features_and_operand_alternatives() ||
	    tcnd_union_fails_loudly_on_unbound_operands() ||
	    tcnd_union_evaluates_not_or_and_set_membership() ||
	    source_ordinals_900_through_1199_are_explicitly_evaluated())
		return 1;
	puts("PASS target feature-domain evaluator");
	return 0;
}
