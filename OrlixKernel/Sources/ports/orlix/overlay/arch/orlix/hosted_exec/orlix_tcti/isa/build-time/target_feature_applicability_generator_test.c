/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_applicability_generator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int output_contains(FILE *file, const char *needle)
{
	char *text;
	long length;
	int result;

	if (fflush(file) || fseek(file, 0, SEEK_END) ||
	    (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET))
		return 0;
	text = calloc((size_t)length + 1U, 1U);
	if (!text)
		return 0;
	result = fread(text, 1, (size_t)length, file) == (size_t)length &&
		strstr(text, needle) != NULL;
	free(text);
	return result;
}

static int outputs_match(FILE *left, FILE *right)
{
	unsigned char left_bytes[1024];
	unsigned char right_bytes[1024];
	size_t left_count;
	size_t right_count;

	if (fflush(left) || fflush(right) || fseek(left, 0, SEEK_SET) ||
	    fseek(right, 0, SEEK_SET))
		return 0;
	do {
		left_count = fread(left_bytes, 1, sizeof(left_bytes), left);
		right_count = fread(right_bytes, 1, sizeof(right_bytes), right);
		if (left_count != right_count ||
		    memcmp(left_bytes, right_bytes, left_count))
			return 0;
	} while (left_count != 0);
	return !ferror(left) && !ferror(right);
}

static int emits_all_pinned_ordinals_with_witnesses(void)
{
	static char name[] = "A";
	static char pmu[] = "PMU", pmdevid[] = "PMDEVID", version[] = "VERSION";
	static uint32_t children[] = { 1U, 2U, 3U, 4U };
	static uint32_t constraints[] = { 7U };
	struct orlix_tcti_feature_parameter parameter = { .name = name };
	struct orlix_tcti_feature_node nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = name },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = pmu },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = pmdevid },
		{ .kind = ORLIX_TCTI_FEATURE_IDENTIFIER, .text = version },
		{ .kind = ORLIX_TCTI_FEATURE_DOT_ATOM, .first_child = 0U,
		  .child_count = 3U },
		{ .kind = ORLIX_TCTI_FEATURE_UINT, .first_child = 3U,
		  .child_count = 1U },
		{ .kind = ORLIX_TCTI_FEATURE_INTEGER, .integer = 1 },
		{ .kind = ORLIX_TCTI_FEATURE_GE, .left = 5U, .right = 6U },
	};
	struct orlix_tcti_target_expr expression = { .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = name };
	struct orlix_tcti_feature_model model = { .parameters = &parameter, .parameter_count = 1U,
		.constraints = constraints, .constraint_count = 1U,
		.nodes = nodes, .node_count = sizeof(nodes) / sizeof(nodes[0]),
		.children = children, .child_count = sizeof(children) / sizeof(children[0]) };
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_feature_sat_audit audit = { 0 };
	struct orlix_tcti_target_feature_sat_error sat_error = { 0 };
	FILE *first;
	FILE *second;
	size_t index;

	inventory.leaves = calloc(ORLIX_TCTI_A64_TARGET_LEAF_COUNT, sizeof(*inventory.leaves));
	EXPECT(inventory.leaves != NULL);
	inventory.leaf_count = ORLIX_TCTI_A64_TARGET_LEAF_COUNT;
	inventory.expressions = &expression;
	inventory.expression_count = 1U;
	for (index = 0; index < inventory.leaf_count; index++) {
		inventory.leaves[index].name = "Leaf";
		inventory.leaves[index].mnemonic = "TEST";
		inventory.leaves[index].operation_id = "op";
		inventory.leaves[index].condition = 0U;
	}
	EXPECT(!orlix_tcti_target_feature_sat_audit(&model, &inventory,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, NULL,
		&audit, &sat_error));
	first = tmpfile();
	EXPECT(first != NULL);
	EXPECT(orlix_tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		first) == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK);
	EXPECT(output_contains(first,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(\"vFAPA2-A\", \"2026-06_rel\", "
		"\"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\", "
		"\"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187\", "
		"\"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874\", "
		"\"ceb8f8c561a5ecdce1a32bda12c7f33fc1b0433bbf035301f2590b6000ccbfe4\""));
	EXPECT(output_contains(first,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(0U, \"Leaf\", \"TEST\", \"op\", 0U, "));
	EXPECT(output_contains(first, "54434e4401"));
	EXPECT(output_contains(first,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(4349U, \"Leaf\", \"TEST\", \"op\", 0U, "));
	EXPECT(output_contains(first,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(1U, 4350U, 0U, 16U)"));
	EXPECT(output_contains(first,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_SYMBOL(0U, 0U, 0U, 4U, 4294967295U, \"\", 128U)"));
	EXPECT(output_contains(first,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_COMMON_VALUES(0U,"));
	second = tmpfile();
	EXPECT(second != NULL);
	EXPECT(orlix_tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		second) == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK);
	EXPECT(outputs_match(first, second));
	fclose(second);
	fclose(first);
	orlix_tcti_target_feature_sat_audit_destroy(&audit);
	free(inventory.leaves);
	return 0;
}

static int fails_closed_for_incomplete_witness(void)
{
	static char name[] = "A";
	struct orlix_tcti_feature_parameter parameter = { .name = name };
	struct orlix_tcti_target_expr expression = { .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true };
	struct orlix_tcti_feature_model model = { .parameters = &parameter, .parameter_count = 1U };
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_feature_sat_audit audit = { 0 };
	FILE *output;

	inventory.leaves = calloc(ORLIX_TCTI_A64_TARGET_LEAF_COUNT, sizeof(*inventory.leaves));
	audit.leaves = calloc(ORLIX_TCTI_A64_TARGET_LEAF_COUNT, sizeof(*audit.leaves));
	EXPECT(inventory.leaves && audit.leaves);
	inventory.leaf_count = ORLIX_TCTI_A64_TARGET_LEAF_COUNT;
	inventory.expressions = &expression;
	inventory.expression_count = 1U;
	audit.leaf_count = ORLIX_TCTI_A64_TARGET_LEAF_COUNT;
	audit.parameter_count = 1U;
	audit.leaves[0] = ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE;
	output = tmpfile();
	EXPECT(output != NULL);
	EXPECT(orlix_tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		output) == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_UNAVAILABLE);
	EXPECT(!output_contains(output, "ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE"));
	fclose(output);

	audit.witnesses = calloc(ORLIX_TCTI_A64_TARGET_LEAF_COUNT,
				 sizeof(*audit.witnesses));
	EXPECT(audit.witnesses != NULL);
	audit.leaves[0] = ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE;
	output = tmpfile();
	EXPECT(output != NULL);
	EXPECT(orlix_tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		output) == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSAT_UNCERTIFIED);
	EXPECT(!output_contains(output,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE"));
	fclose(output);
	audit.leaves[0] = ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE;

	output = tmpfile();
	EXPECT(output != NULL);
	EXPECT(orlix_tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		output) == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_INVALID);
	EXPECT(!output_contains(output, "ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE"));
	fclose(output);

	free(audit.witnesses);
	free(audit.leaves);
	free(inventory.leaves);
	return 0;
}

int main(void)
{
	if (emits_all_pinned_ordinals_with_witnesses() ||
	    fails_closed_for_incomplete_witness())
		return 1;
	puts("PASS target feature applicability generator");
	return 0;
}
