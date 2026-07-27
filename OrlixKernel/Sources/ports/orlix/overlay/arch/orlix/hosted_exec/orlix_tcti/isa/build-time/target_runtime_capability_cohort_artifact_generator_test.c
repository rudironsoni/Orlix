/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#define TARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_NO_MAIN
#include "target_runtime_capability_cohort_artifact_generator.c"

#include <stdio.h>

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int read_file(const char *path, char **data, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;

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
		return -1;
	}
	(*data)[size] = '\0';
	*length = (size_t)size;
	return 0;
}

static char *capture(const char *instructions, size_t instructions_length,
	const char *features, size_t features_length)
{
	FILE *file = tmpfile();
	long length;
	char *text;

	if (!file || orlix_tcti_runtime_capability_cohort_artifact_emit(instructions,
		instructions_length, features, features_length, file)) {
		if (file)
			fclose(file);
		return NULL;
	}
	if (fflush(file) || fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return NULL;
	}
	text = malloc((size_t)length + 1U);
	if (!text || fread(text, 1, (size_t)length, file) != (size_t)length) {
		free(text);
		fclose(file);
		return NULL;
	}
	text[length] = '\0';
	fclose(file);
	return text;
}

static int model_helpers_reject_cycle_and_deduplicate(void)
{
	struct cohort_model model = { 0 };
	struct orlix_tcti_target_expr expressions[2] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_AND, .left = 1U },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_AND, .left = 0U },
	};
	struct orlix_tcti_target_inventory inventory = { .expressions = expressions,
		.expression_count = 2U };
	struct orlix_tcti_feature_parameter parameters[] = { { .name = "FEAT_X" } };
	struct orlix_tcti_feature_model features = { .parameters = parameters,
		.parameter_count = 1U };
	uint8_t visited[2] = { 0 };

	EXPECT(!add_member(&model, 1U, 0U, 12U, 3U));
	EXPECT(!add_member(&model, 1U, 0U, 99U, 2U));
	EXPECT(model.count == 1U);
	EXPECT(collect_expression(&inventory, &features, &model, 0U, 0U,
		visited) == -1);
	free(model.memberships);
	return 0;
}

static int emitted_memberships_are_sorted_and_provenanced(const char *text)
{
	const char *cursor = text;
	unsigned int order, leaf, parameter, previous_leaf = 0U;
	unsigned int previous_parameter = 0U;
	size_t rows = 0;

	while ((cursor = strstr(cursor,
		"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(")) != NULL) {
		const char *tail = strchr(cursor, ')');
		unsigned long offset, length;

		EXPECT(tail);
		EXPECT(sscanf(cursor,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(%uU, %uU, %uU,",
			&order, &leaf, &parameter) == 3);
		EXPECT(order == rows && leaf < ORLIX_TCTI_A64_TARGET_LEAF_COUNT);
		if (rows && leaf == previous_leaf)
			EXPECT(parameter > previous_parameter);
		else if (rows)
			EXPECT(leaf > previous_leaf);
		/* The final two unsigned fields are the exact AST.FEAT source span. */
		{
			const char *span = strstr(cursor,
				"ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED, ");

			EXPECT(span && span < tail);
			EXPECT(sscanf(span,
				"ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED, %luU, %luU",
				&offset, &length) == 2 && offset && length);
		}
		previous_leaf = leaf;
		previous_parameter = parameter;
		rows++;
		cursor = tail + 1;
	}
	return rows == 5592U ? 0 : -1;
}

static int unknown_feature_is_rejected(void)
{
	struct cohort_model model = { 0 };
	struct orlix_tcti_target_expr expression = {
		.kind = ORLIX_TCTI_TARGET_EXPR_FEATURE,
		.text = "FEAT_UNKNOWN",
	};
	struct orlix_tcti_target_inventory inventory = {
		.expressions = &expression,
		.expression_count = 1U,
	};
	struct orlix_tcti_feature_parameter parameter = { .name = "FEAT_KNOWN" };
	struct orlix_tcti_feature_model features = {
		.parameters = &parameter,
		.parameter_count = 1U,
	};
	uint8_t visited = 0;

	EXPECT(collect_expression(&inventory, &features, &model, 0U, 0U,
		&visited) == -2);
	free(model.memberships);
	return 0;
}

int main(int argc, char **argv)
{
	char *instructions = NULL, *features = NULL, *first = NULL, *second = NULL;
	size_t instructions_length, features_length;
	int status = 1;

	if (argc != 3 || read_file(argv[1], &instructions, &instructions_length) ||
	    read_file(argv[2], &features, &features_length) ||
	    model_helpers_reject_cycle_and_deduplicate())
		goto out;
	first = capture(instructions, instructions_length, features, features_length);
	second = capture(instructions, instructions_length, features, features_length);
	if (!first || !second || strcmp(first, second) ||
	    !strstr(first, "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(4350U, 5592U, 5592U, 5592U)") ||
	    !strstr(first, "ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED") ||
	    emitted_memberships_are_sorted_and_provenanced(first) ||
	    unknown_feature_is_rejected())
		goto out;
	status = 0;
out:
	free(second);
	free(first);
	free(features);
	free(instructions);
	return status;
}
