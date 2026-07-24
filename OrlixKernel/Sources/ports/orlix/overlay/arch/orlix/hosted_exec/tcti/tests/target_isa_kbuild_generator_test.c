/* SPDX-License-Identifier: GPL-2.0-only */
#define TCTI_A64_KBUILD_GENERATOR_NO_BUILTIN_INPUT
#include "target_isa_kbuild_generator.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

struct fixture {
	struct tcti_a64_kbuild_metadata metadata;
	struct tcti_a64_kbuild_source_row *source;
	struct tcti_a64_kbuild_classification_row *classification;
	char (*names)[24];
};

static int fixture_init(struct fixture *fixture)
{
	size_t index;

	fixture->metadata = (struct tcti_a64_kbuild_metadata){
		.architecture = TCTI_A64_KBUILD_ARCHITECTURE,
		.build = TCTI_A64_KBUILD_BUILD,
		.reference = TCTI_A64_KBUILD_REFERENCE,
		.schema = TCTI_A64_KBUILD_SCHEMA,
		.instructions_sha256 =
			TCTI_A64_KBUILD_INSTRUCTIONS_SHA256,
		.source_count = TCTI_A64_KBUILD_SOURCE_COUNT,
	};
	fixture->source = calloc(TCTI_A64_KBUILD_SOURCE_COUNT,
				 sizeof(*fixture->source));
	fixture->classification =
		calloc(TCTI_A64_KBUILD_SOURCE_COUNT,
		       sizeof(*fixture->classification));
	fixture->names = calloc(TCTI_A64_KBUILD_SOURCE_COUNT,
			       sizeof(*fixture->names));
	CHECK(fixture->source != NULL);
	CHECK(fixture->classification != NULL);
	CHECK(fixture->names != NULL);
	for (index = 0; index < TCTI_A64_KBUILD_SOURCE_COUNT; index++) {
		CHECK(snprintf(fixture->names[index],
			       sizeof(fixture->names[index]), "fixture_%04zu",
			       index) > 0);
		fixture->source[index] =
			(struct tcti_a64_kbuild_source_row){
				.ordinal = (uint32_t)index,
				.name = fixture->names[index],
				.mnemonic = "FIXTURE",
				.operation = "fixture_operation",
				.mask = UINT32_MAX,
				.pattern = (uint32_t)index,
				.condition_tcnd_hex = "54434e4401",
			};
		fixture->classification[index] =
			(struct tcti_a64_kbuild_classification_row){
				.name = fixture->names[index],
				.classification = index ?
					TCTI_A64_KBUILD_REQUIRED :
					TCTI_A64_KBUILD_UNCLASSIFIED,
				.relation = TCTI_A64_KBUILD_RELATION_NONE,
				.canonical = "",
				.evidence = "",
				.proof = "",
			};
	}
	return 0;
}

static void fixture_destroy(struct fixture *fixture)
{
	free(fixture->source);
	free(fixture->classification);
	free(fixture->names);
	*fixture = (struct fixture){ 0 };
}

static long stream_length(FILE *stream)
{
	if (fflush(stream) || fseek(stream, 0, SEEK_END))
		return -1;
	return ftell(stream);
}

static int streams_equal(FILE *left, FILE *right)
{
	unsigned char left_bytes[4096];
	unsigned char right_bytes[4096];
	size_t left_count;
	size_t right_count;

	CHECK(!fseek(left, 0, SEEK_SET));
	CHECK(!fseek(right, 0, SEEK_SET));
	for (;;) {
		left_count = fread(left_bytes, 1, sizeof(left_bytes), left);
		right_count = fread(right_bytes, 1, sizeof(right_bytes), right);
		CHECK(left_count == right_count);
		CHECK(!memcmp(left_bytes, right_bytes, left_count));
		if (!left_count)
			break;
	}
	CHECK(!ferror(left));
	CHECK(!ferror(right));
	return 0;
}

static int stream_contains(FILE *stream, const char *needle)
{
	long length = stream_length(stream);
	char *bytes;
	int found;

	CHECK(length >= 0);
	CHECK(!fseek(stream, 0, SEEK_SET));
	bytes = malloc((size_t)length + 1U);
	CHECK(bytes != NULL);
	CHECK(fread(bytes, 1, (size_t)length, stream) == (size_t)length);
	bytes[length] = '\0';
	found = strstr(bytes, needle) != NULL;
	free(bytes);
	return found ? 0 : -1;
}

static int expect_failure(struct fixture *fixture,
			  enum tcti_a64_kbuild_generator_error expected)
{
	FILE *output = tmpfile();

	CHECK(output != NULL);
	CHECK(tcti_a64_kbuild_generate_header(
		      &fixture->metadata, fixture->source,
		      TCTI_A64_KBUILD_SOURCE_COUNT, fixture->classification,
		      TCTI_A64_KBUILD_SOURCE_COUNT, output) == expected);
	CHECK(stream_length(output) == 0);
	fclose(output);
	return 0;
}

static int deterministic_fixed_width_artifact(void)
{
	struct fixture fixture = { 0 };
	FILE *first;
	FILE *second;

	CHECK(!fixture_init(&fixture));
	first = tmpfile();
	second = tmpfile();
	CHECK(first != NULL);
	CHECK(second != NULL);
	CHECK(tcti_a64_kbuild_generate_header(
		      &fixture.metadata, fixture.source,
		      TCTI_A64_KBUILD_SOURCE_COUNT, fixture.classification,
		      TCTI_A64_KBUILD_SOURCE_COUNT, first) ==
	      TCTI_A64_KBUILD_GENERATOR_OK);
	CHECK(tcti_a64_kbuild_generate_header(
		      &fixture.metadata, fixture.source,
		      TCTI_A64_KBUILD_SOURCE_COUNT, fixture.classification,
		      TCTI_A64_KBUILD_SOURCE_COUNT, second) ==
	      TCTI_A64_KBUILD_GENERATOR_OK);
	CHECK(!streams_equal(first, second));
	CHECK(!stream_contains(first,
		"#define TCTI_A64_GENERATED_SOURCE_COUNT 4350U"));
	CHECK(!stream_contains(first,
		"#define TCTI_A64_GENERATED_UNCLASSIFIED_COUNT 1U"));
	CHECK(!stream_contains(first,
		"#define TCTI_A64_GENERATED_CLASSIFICATION_COMPLETE 0U"));
	CHECK(!stream_contains(first,
		"#define TCTI_A64_GENERATED_ISA_COMPLETE 0U"));
	fclose(first);
	fclose(second);
	fixture_destroy(&fixture);
	return 0;
}

static int adversarial_c_fixtures(void)
{
	struct fixture fixture = { 0 };
	const char *saved_name;
	uint32_t saved_ordinal;
	enum tcti_a64_kbuild_classification saved_classification;
	enum tcti_a64_kbuild_relation saved_relation;
	const char *saved_canonical;
	FILE *output;

	CHECK(!fixture_init(&fixture));
	output = tmpfile();
	CHECK(output != NULL);
	CHECK(tcti_a64_kbuild_generate_header(
		      &fixture.metadata, fixture.source,
		      TCTI_A64_KBUILD_SOURCE_COUNT - 1U,
		      fixture.classification, TCTI_A64_KBUILD_SOURCE_COUNT,
		      output) == TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH);
	CHECK(stream_length(output) == 0);
	fclose(output);

	fixture.metadata.instructions_sha256 =
		"b1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe";
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH));
	fixture.metadata.instructions_sha256 =
		TCTI_A64_KBUILD_INSTRUCTIONS_SHA256;

	saved_ordinal = fixture.source[17].ordinal;
	fixture.source[17].ordinal = 16U;
	CHECK(!expect_failure(&fixture, TCTI_A64_KBUILD_GENERATOR_BAD_ORDINAL));
	fixture.source[17].ordinal = saved_ordinal;

	saved_name = fixture.classification[23].name;
	fixture.classification[23].name = "different_leaf";
	CHECK(!expect_failure(
		&fixture, TCTI_A64_KBUILD_GENERATOR_CLASSIFICATION_MISMATCH));
	fixture.classification[23].name = saved_name;

	saved_name = fixture.source[31].name;
	fixture.source[31].name = fixture.source[30].name;
	fixture.classification[31].name = fixture.source[30].name;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_DUPLICATE_SOURCE));
	fixture.source[31].name = saved_name;
	fixture.classification[31].name = saved_name;

	saved_classification = fixture.classification[1].classification;
	fixture.classification[1].classification =
		(enum tcti_a64_kbuild_classification)99;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION));
	fixture.classification[1].classification = saved_classification;

	saved_relation = fixture.classification[2].relation;
	saved_canonical = fixture.classification[2].canonical;
	fixture.classification[2].relation = TCTI_A64_KBUILD_RELATION_ALIAS;
	fixture.classification[2].canonical = "";
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION));
	fixture.classification[2].relation = saved_relation;
	fixture.classification[2].canonical = saved_canonical;

	fixture.source[3].condition_tcnd_hex = "not-tcnd";
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW));
	fixture_destroy(&fixture);
	return 0;
}

int main(void)
{
	CHECK(!deterministic_fixed_width_artifact());
	CHECK(!adversarial_c_fixtures());
	puts("target ISA Kbuild generator C-fixture tests: passed");
	return 0;
}
