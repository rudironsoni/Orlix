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
	struct tcti_feature_parameter parameter = { .name = name };
	struct tcti_feature_node node = { .kind = TCTI_FEATURE_IDENTIFIER, .text = name };
	struct tcti_target_expr expression = { .kind = TCTI_TARGET_EXPR_FEATURE, .text = name };
	struct tcti_feature_model model = { .parameters = &parameter, .parameter_count = 1U,
		.nodes = &node, .node_count = 1U };
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_feature_sat_audit audit = { 0 };
	struct tcti_target_feature_sat_error sat_error = { 0 };
	FILE *first;
	FILE *second;
	size_t index;

	inventory.leaves = calloc(TCTI_A64_TARGET_LEAF_COUNT, sizeof(*inventory.leaves));
	EXPECT(inventory.leaves != NULL);
	inventory.leaf_count = TCTI_A64_TARGET_LEAF_COUNT;
	inventory.expressions = &expression;
	inventory.expression_count = 1U;
	for (index = 0; index < inventory.leaf_count; index++) {
		inventory.leaves[index].name = "Leaf";
		inventory.leaves[index].mnemonic = "TEST";
		inventory.leaves[index].operation_id = "op";
		inventory.leaves[index].condition = 0U;
	}
	EXPECT(!tcti_target_feature_sat_audit(&model, &inventory,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, &audit, &sat_error));
	first = tmpfile();
	EXPECT(first != NULL);
	EXPECT(tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		first) == TCTI_TARGET_FEATURE_APPLICABILITY_OK);
	EXPECT(output_contains(first,
		"TCTI_A64_FEATURE_APPLICABILITY_SOURCE(\"vFAPA1-A\", \"2026-06_rel\""));
	EXPECT(output_contains(first,
		"TCTI_A64_FEATURE_APPLICABILITY_ROW(0U, \"Leaf\", \"TEST\", \"op\", 0U, 1U, \"\\001\", 1U)"));
	EXPECT(output_contains(first,
		"TCTI_A64_FEATURE_APPLICABILITY_ROW(4349U, \"Leaf\", \"TEST\", \"op\", 0U, 1U, \"\\001\", 1U)"));
	second = tmpfile();
	EXPECT(second != NULL);
	EXPECT(tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		second) == TCTI_TARGET_FEATURE_APPLICABILITY_OK);
	EXPECT(outputs_match(first, second));
	fclose(second);
	fclose(first);
	tcti_target_feature_sat_audit_destroy(&audit);
	free(inventory.leaves);
	return 0;
}

static int fails_closed_for_incomplete_witness(void)
{
	static char name[] = "A";
	struct tcti_feature_parameter parameter = { .name = name };
	struct tcti_target_expr expression = { .kind = TCTI_TARGET_EXPR_BOOL, .boolean = true };
	struct tcti_feature_model model = { .parameters = &parameter, .parameter_count = 1U };
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_feature_sat_audit audit = { 0 };
	FILE *output;

	inventory.leaves = calloc(TCTI_A64_TARGET_LEAF_COUNT, sizeof(*inventory.leaves));
	audit.leaves = calloc(TCTI_A64_TARGET_LEAF_COUNT, sizeof(*audit.leaves));
	EXPECT(inventory.leaves && audit.leaves);
	inventory.leaf_count = TCTI_A64_TARGET_LEAF_COUNT;
	inventory.expressions = &expression;
	inventory.expression_count = 1U;
	audit.leaf_count = TCTI_A64_TARGET_LEAF_COUNT;
	audit.parameter_count = 1U;
	audit.leaves[0] = TCTI_TARGET_FEATURE_SAT_APPLICABLE;
	output = tmpfile();
	EXPECT(output != NULL);
	EXPECT(tcti_target_feature_applicability_emit(&model, &inventory, &audit,
		output) == TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_UNAVAILABLE);
	EXPECT(!output_contains(output, "TCTI_A64_FEATURE_APPLICABILITY_SOURCE"));
	fclose(output);
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
