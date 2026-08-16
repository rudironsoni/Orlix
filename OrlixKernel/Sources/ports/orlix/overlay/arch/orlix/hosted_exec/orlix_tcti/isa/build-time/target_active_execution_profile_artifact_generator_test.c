/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#define TARGET_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_NO_MAIN
#include "target_active_execution_profile_artifact_generator.c"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

struct text {
	char *data;
	size_t length;
	size_t capacity;
};

struct fixture {
	struct text source_manifest;
	struct text applicability;
	struct text cohort;
	struct text profile;
	struct text promotion;
	struct text proof;
	struct text classification;
	struct text registry;
};

static const char *const fixture_condition =
	"54434e4401070000004e07000000220700000017070000000c0100000001010100000001010100000001010100000001010800000022020000000c00000008464541545f535645020000000c00000008464541545f534d45";

static int appendf(struct text *text, const char *format, ...)
{
	va_list arguments;
	va_list copy;
	int needed;

	va_start(arguments, format);
	va_copy(copy, arguments);
	needed = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	if (needed < 0 || (size_t)needed > SIZE_MAX - text->length - 1U) {
		va_end(arguments);
		return -1;
	}
	if (text->length + (size_t)needed + 1U > text->capacity) {
		size_t capacity = text->capacity ? text->capacity : 256U;
		char *data;

		while (capacity < text->length + (size_t)needed + 1U) {
			if (capacity > SIZE_MAX / 2U) {
				va_end(arguments);
				return -1;
			}
			capacity *= 2U;
		}
		data = realloc(text->data, capacity);
		if (!data) {
			va_end(arguments);
			return -1;
		}
		text->data = data;
		text->capacity = capacity;
	}
	(void)vsnprintf(text->data + text->length, (size_t)needed + 1U,
		format, arguments);
	va_end(arguments);
	text->length += (size_t)needed;
	return 0;
}

static void text_destroy(struct text *text)
{
	free(text->data);
	*text = (struct text) { 0 };
}

static int text_copy(struct text *destination, const struct text *source)
{
	text_destroy(destination);
	if (!source->length)
		return 0;
	destination->data = malloc(source->length + 1U);
	if (!destination->data)
		return -1;
	memcpy(destination->data, source->data, source->length + 1U);
	destination->length = source->length;
	destination->capacity = source->length + 1U;
	return 0;
}

static int replace_once(struct text *text, const char *old, const char *replacement)
{
	char *match = strstr(text->data, old);
	struct text replaced = { 0 };

	if (!match || appendf(&replaced, "%.*s%s%s",
		(int)(match - text->data), text->data, replacement,
		match + strlen(old)) ||
		!replaced.data)
		return -1;
	text_destroy(text);
	*text = replaced;
	return 0;
}

static int replace_all(struct text *text, const char *old,
	const char *replacement)
{
	if (!old[0] || !strcmp(old, replacement))
		return old[0] ? 0 : -1;
	while (strstr(text->data, old) &&
		replace_once(text, old, replacement) == 0)
		;
	return strstr(text->data, old) ? -1 : 0;
}

static int build_source_manifest(struct text *source)
{
	unsigned int ordinal;

	if (appendf(source,
		"ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(\"vFATAp1-A\", \"818\", \"2026-06_rel\", \"2.9.5\", \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\", 4350, \"2026-06-24 17:12:14\", 100000U)\n"))
		return -1;
	for (ordinal = 0; ordinal < 4350U; ordinal++)
		if (appendf(source,
			"ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(%u, \"%s\", \"ADD\", \"add\", 0xffU, 0x0U, \"%s\", %uU, 1U)\n",
			ordinal, ordinal == 2166U ? "leaf_x" : "source_leaf", fixture_condition,
			ordinal + 1U))
			return -1;
	return 0;
}

static int build_applicability(struct text *applicability)
{
	unsigned int ordinal;

	if (appendf(applicability,
		"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_SOURCE(\"vFAPA2-A\", \"2026-06_rel\", \"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\", \"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187\", \"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874\", \"ceb8f8c561a5ecdce1a32bda12c7f33fc1b0433bbf035301f2590b6000ccbfe4\", 4350U, 409U)\n") ||
		appendf(applicability,
			"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_CERTIFICATE_COUNTS(377U, 4350U, 364U, 6032U)\n"))
		return -1;
	for (ordinal = 0; ordinal < 4350U; ordinal++)
		if (appendf(applicability,
			"ORLIX_TCTI_A64_FEATURE_APPLICABILITY_ROW(%uU, \"%s\", \"ADD\", \"add\", 0U, 88U, 1U, \"%s\", \"\", 0U, 0U, 0U, 0x1ULL)\n",
			ordinal, ordinal == 2166U ? "leaf_x" : "source_leaf",
			fixture_condition))
			return -1;
	return 0;
}

static int build_cohort(struct text *cohort)
{
	unsigned int leaf;
	unsigned int membership;
	unsigned int first = 0;

	if (appendf(cohort,
		"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(4350U, 5592U, 5592U, 5592U)\n"))
		return -1;
	for (leaf = 0; leaf < 4350U; leaf++) {
		unsigned int count = leaf < 1242U ? 2U : 1U;

		if (appendf(cohort,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(%uU, %uU, %uU)\n",
			leaf, first, count))
			return -1;
		first += count;
	}
	membership = 0;
	for (leaf = 0; leaf < 4350U; leaf++) {
		unsigned int count = leaf < 1242U ? 2U : 1U;
		unsigned int parameter;

		for (parameter = 0; parameter < count; parameter++) {
			const char *feature = leaf == 2166U ? "FEAT_SVE" : "FEAT_OTHER";

			if (appendf(cohort,
				"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(%uU, %uU, %uU, \"%s\", ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED, 1U, 1U)\n",
				membership++, leaf, parameter, feature))
				return -1;
		}
	}
	return membership == 5592U ? 0 : -1;
}

static int build_fixture(struct fixture *fixture)
{
	*fixture = (struct fixture) { 0 };
	if (build_source_manifest(&fixture->source_manifest) ||
		build_applicability(&fixture->applicability) ||
		build_cohort(&fixture->cohort) ||
		appendf(&fixture->profile,
			"ORLIX_TCTI_A64_ACTIVE_EXECUTION_FEATURE(FEAT_SVE, ENABLED, \"feature-x\")\n") ||
		appendf(&fixture->promotion,
			"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROMOTION(FEAT_SVE, \"feature-x\", 2166U, \"leaf_x\", \"kunit:source-leaf-udf-undefined\")\n") ||
		appendf(&fixture->proof,
			"ORLIX_TCTI_A64_SOURCE_BOUND_PROOF(2166U, \"kunit:source-leaf-udf-undefined\")\n") ||
		appendf(&fixture->classification,
			"ORLIX_TCTI_A64_TARGET_CLASSIFICATION(leaf_x, REQUIRED, ORLIX_TCTI_A64_TARGET_RELATION_NONE, \"\", \"\", \"kunit:source-leaf-udf-undefined\")\n") ||
		appendf(&fixture->registry,
			"ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING(2166U, \"kunit:source-leaf-udf-undefined\")\n")) {
		text_destroy(&fixture->source_manifest);
		text_destroy(&fixture->applicability);
		text_destroy(&fixture->cohort);
		text_destroy(&fixture->profile);
		text_destroy(&fixture->promotion);
		text_destroy(&fixture->proof);
		text_destroy(&fixture->classification);
		text_destroy(&fixture->registry);
		return -1;
	}
	return 0;
}

static void destroy_fixture(struct fixture *fixture)
{
	text_destroy(&fixture->source_manifest);
	text_destroy(&fixture->applicability);
	text_destroy(&fixture->cohort);
	text_destroy(&fixture->profile);
	text_destroy(&fixture->promotion);
	text_destroy(&fixture->proof);
	text_destroy(&fixture->classification);
	text_destroy(&fixture->registry);
}

static enum orlix_tcti_active_execution_profile_artifact_generator_error emit_fixture(
	const struct text *source_manifest,
	const struct text *applicability,
	const struct text *cohort, const struct text *profile,
	const struct text *promotion, const struct text *proof,
	const struct text *classification, const struct text *registry,
	char **output_bytes)
{
	FILE *output = tmpfile();
	enum orlix_tcti_active_execution_profile_artifact_generator_error result;
	long length;

	*output_bytes = NULL;
	if (!output)
		return ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_IO;
	result = orlix_tcti_active_execution_profile_artifact_emit(
		"\"FEAT_SVE\" \"FEAT_OTHER\"", strlen("\"FEAT_SVE\" \"FEAT_OTHER\""),
		applicability->data, applicability->length,
		profile->data, profile->length, promotion->data, promotion->length,
		"schema", 6U, proof->data, proof->length, cohort->data, cohort->length,
		classification->data, classification->length, registry->data,
		registry->length, source_manifest->data, source_manifest->length,
		output);
	if (result != ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK) {
		fclose(output);
		return result;
	}
	if (fflush(output) || fseek(output, 0L, SEEK_END) || (length = ftell(output)) < 0 ||
		fseek(output, 0L, SEEK_SET)) {
		fclose(output);
		return ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_IO;
	}
	*output_bytes = calloc((size_t)length + 1U, 1U);
	if (!*output_bytes || fread(*output_bytes, 1U, (size_t)length, output) !=
		(size_t)length) {
		free(*output_bytes);
		*output_bytes = NULL;
		result = ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_IO;
	}
	fclose(output);
	return result;
}

static int expect_rejected_condition(const struct fixture *fixture,
	const char *condition, const char *condition_length)
{
	struct text source = { 0 };
	struct text applicability = { 0 };
	char *output = NULL;
	enum orlix_tcti_active_execution_profile_artifact_generator_error result;

	if (text_copy(&source, &fixture->source_manifest) ||
		text_copy(&applicability, &fixture->applicability) ||
		replace_all(&source, fixture_condition, condition) ||
		replace_all(&applicability, fixture_condition, condition) ||
		replace_all(&applicability, "88U", condition_length)) {
		text_destroy(&source);
		text_destroy(&applicability);
		return -1;
	}
	result = emit_fixture(&source, &applicability, &fixture->cohort,
		&fixture->profile, &fixture->promotion, &fixture->proof,
		&fixture->classification, &fixture->registry, &output);
	free(output);
	text_destroy(&source);
	text_destroy(&applicability);
	return result == ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK ?
		-1 : 0;
}

static int valid_enabled_and_mutations(void)
{
	struct fixture fixture;
	struct text mutation = { 0 };
	char *first = NULL;
	char *second = NULL;
	enum orlix_tcti_active_execution_profile_artifact_generator_error result;

	EXPECT(build_fixture(&fixture) == 0);
	result = emit_fixture(&fixture.source_manifest, &fixture.applicability,
		&fixture.cohort,
		&fixture.profile, &fixture.promotion, &fixture.proof,
		&fixture.classification, &fixture.registry, &first);
	EXPECT(result == ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	EXPECT(first != NULL);
	EXPECT(strstr(first,
		"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_COUNTS(1U, 1U, 0U)"));
	EXPECT(strstr(first,
		"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE_MANIFEST(4350U, 100000U,"));
	EXPECT(emit_fixture(&fixture.source_manifest, &fixture.applicability,
		&fixture.cohort,
		&fixture.profile, &fixture.promotion, &fixture.proof,
		&fixture.classification, &fixture.registry, &second) ==
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	EXPECT(second != NULL && !strcmp(first, second));
	free(second);
	second = NULL;

	/* A positive feature join must not survive a false Boolean conjunct. */
	EXPECT(expect_rejected_condition(&fixture,
		"54434e44010700000017020000000c00000008464541545f535645010000000100",
		"33U") == 0);
	/* Negation and an OR made true by an unrelated constant are not resolved. */
	EXPECT(expect_rejected_condition(&fixture,
		"54434e44010600000011020000000c00000008464541545f535645",
		"27U") == 0);
	EXPECT(expect_rejected_condition(&fixture,
		"54434e44010800000017020000000c00000008464541545f535645010000000101",
		"33U") == 0);

	EXPECT(text_copy(&mutation, &fixture.cohort) == 0);
	EXPECT(replace_once(&mutation, "5592U, 5592U, 5592U", "5591U, 5591U, 5591U") == 0);
	EXPECT(emit_fixture(&fixture.source_manifest, &fixture.applicability,
		&mutation,
		&fixture.profile, &fixture.promotion, &fixture.proof,
		&fixture.classification, &fixture.registry, &second) !=
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	text_destroy(&mutation);
	free(second);
	second = NULL;

	EXPECT(text_copy(&mutation, &fixture.promotion) == 0);
	EXPECT(appendf(&mutation,
			"ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROMOTION(FEAT_SVE, \"feature-x\", 2167U, \"source_leaf\", \"kunit:source-leaf-udf-undefined\")\n") == 0);
	EXPECT(emit_fixture(&fixture.source_manifest, &fixture.applicability,
		&fixture.cohort,
		&fixture.profile, &mutation, &fixture.proof,
		&fixture.classification, &fixture.registry, &second) !=
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	text_destroy(&mutation);
	free(second);
	second = NULL;

	EXPECT(text_copy(&mutation, &fixture.registry) == 0);
	EXPECT(replace_once(&mutation, "kunit:source-leaf-udf-undefined", "stale-proof") == 0);
	EXPECT(emit_fixture(&fixture.source_manifest, &fixture.applicability,
		&fixture.cohort,
		&fixture.profile, &fixture.promotion, &fixture.proof,
		&fixture.classification, &mutation, &second) !=
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	text_destroy(&mutation);
	free(second);
	second = NULL;

	EXPECT(text_copy(&mutation, &fixture.source_manifest) == 0);
	EXPECT(replace_once(&mutation,
		"54434e4401070000004e07000000220700000017070000000c010000000101",
		"bad") == 0);
	EXPECT(emit_fixture(&mutation, &fixture.applicability, &fixture.cohort,
		&fixture.profile,
		&fixture.promotion, &fixture.proof, &fixture.classification,
		&fixture.registry, &second) !=
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	text_destroy(&mutation);
	free(second);
	second = NULL;

	EXPECT(text_copy(&mutation, &fixture.applicability) == 0);
	EXPECT(replace_once(&mutation, "ceb8f8c561a5ecdce1a32bda12c7f33fc1b0433bbf035301f2590b6000ccbfe4",
		"0000000000000000000000000000000000000000000000000000000000000000") == 0);
	EXPECT(emit_fixture(&fixture.source_manifest, &mutation, &fixture.cohort,
		&fixture.profile, &fixture.promotion, &fixture.proof,
		&fixture.classification, &fixture.registry, &second) !=
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	text_destroy(&mutation);
	free(second);
	second = NULL;

	EXPECT(text_copy(&mutation, &fixture.applicability) == 0);
	EXPECT(replace_once(&mutation, "leaf_x", "unknown") == 0);
	EXPECT(emit_fixture(&fixture.source_manifest, &mutation, &fixture.cohort,
		&fixture.profile, &fixture.promotion, &fixture.proof,
		&fixture.classification, &fixture.registry, &second) !=
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	text_destroy(&mutation);
	free(second);
	second = NULL;

	EXPECT(text_copy(&mutation, &fixture.proof) == 0);
	EXPECT(replace_once(&mutation, "2166U", "2167U") == 0);
	EXPECT(emit_fixture(&fixture.source_manifest, &fixture.applicability,
		&fixture.cohort,
		&fixture.profile, &fixture.promotion, &mutation,
		&fixture.classification, &fixture.registry, &second) !=
		ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK);
	text_destroy(&mutation);
	free(second);
	free(first);
	destroy_fixture(&fixture);
	return 0;
}

int main(void)
{
	return valid_enabled_and_mutations() ? 1 : 0;
}
