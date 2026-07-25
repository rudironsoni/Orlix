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
	struct tcti_a64_kbuild_system_accessor_metadata accessor_metadata;
	struct tcti_a64_kbuild_system_accessor_row *accessors;
	char (*names)[24];
	char (*accessor_names)[32];
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
		.timestamp = TCTI_A64_KBUILD_INSTRUCTIONS_TIMESTAMP,
		.source_byte_length =
			TCTI_A64_KBUILD_INSTRUCTIONS_SOURCE_BYTES,
		.source_count = TCTI_A64_KBUILD_SOURCE_COUNT,
	};
	fixture->source = calloc(TCTI_A64_KBUILD_SOURCE_COUNT,
				 sizeof(*fixture->source));
	fixture->classification =
		calloc(TCTI_A64_KBUILD_SOURCE_COUNT,
		       sizeof(*fixture->classification));
	fixture->names = calloc(TCTI_A64_KBUILD_SOURCE_COUNT,
			       sizeof(*fixture->names));
	fixture->accessors = calloc(TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT,
				   sizeof(*fixture->accessors));
	fixture->accessor_names =
		calloc(TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT,
		       sizeof(*fixture->accessor_names));
	CHECK(fixture->source != NULL);
	CHECK(fixture->classification != NULL);
	CHECK(fixture->names != NULL);
	CHECK(fixture->accessors != NULL);
	CHECK(fixture->accessor_names != NULL);
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
				.source_offset = (uint32_t)index,
				.source_length = 1U,
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
	fixture->accessor_metadata =
		(struct tcti_a64_kbuild_system_accessor_metadata){
			.architecture = TCTI_A64_KBUILD_ARCHITECTURE,
			.build = TCTI_A64_KBUILD_BUILD,
			.reference = TCTI_A64_KBUILD_REFERENCE,
			.schema = TCTI_A64_KBUILD_SCHEMA,
			.timestamp = TCTI_A64_KBUILD_REGISTERS_TIMESTAMP,
			.registers_sha256 = TCTI_A64_KBUILD_REGISTERS_SHA256,
			.accessor_count = TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT,
			.mapped_count = TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT,
		};
	for (index = 0; index < TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT; index++) {
		CHECK(snprintf(fixture->accessor_names[index],
			       sizeof(fixture->accessor_names[index]),
			       "A64.fixture_%04zu", index) > 0);
		fixture->accessors[index] =
			(struct tcti_a64_kbuild_system_accessor_row){
				.accessor_index = (uint32_t)index,
				.encoding_index = (uint32_t)index,
				.name = fixture->accessor_names[index],
				.generic_leaf = fixture->source[index].name,
				.direction =
					TCTI_A64_KBUILD_SYSTEM_ACCESSOR_DIRECTION_READ,
				.disposition =
					TCTI_A64_KBUILD_SYSTEM_ACCESSOR_MAPPED,
				.selector_count = 5U,
				.condition_expression = (uint32_t)index,
				.selector_identity = UINT64_C(0x1000000000000000) + index,
				.condition_identity = UINT64_C(0x2000000000000000) + index,
				.accessor_source_offset = (uint32_t)(index + 1U),
				.accessor_source_length = 1U,
				.encoding_source_offset = (uint32_t)(index + 2U),
				.encoding_source_length = 1U,
				.condition_source_offset = (uint32_t)(index + 3U),
				.condition_source_length = 1U,
			};
	}
	fixture->accessor_metadata.reconciliation_identity =
		tcti_a64_kbuild_system_accessor_identity(
			fixture->accessors, TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT);
	return 0;
}

static void fixture_destroy(struct fixture *fixture)
{
	free(fixture->source);
	free(fixture->classification);
	free(fixture->names);
	free(fixture->accessors);
	free(fixture->accessor_names);
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
		      TCTI_A64_KBUILD_SOURCE_COUNT,
		      &fixture->accessor_metadata, fixture->accessors,
		      TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT, output) == expected);
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
		      TCTI_A64_KBUILD_SOURCE_COUNT,
		      &fixture.accessor_metadata, fixture.accessors,
		      TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT, first) ==
	      TCTI_A64_KBUILD_GENERATOR_OK);
	CHECK(tcti_a64_kbuild_generate_header(
		      &fixture.metadata, fixture.source,
		      TCTI_A64_KBUILD_SOURCE_COUNT, fixture.classification,
		      TCTI_A64_KBUILD_SOURCE_COUNT,
		      &fixture.accessor_metadata, fixture.accessors,
		      TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT, second) ==
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
	CHECK(!stream_contains(first,
		"#define TCTI_A64_GENERATED_SYSTEM_ACCESSOR_COUNT 2014U"));
	CHECK(!stream_contains(first,
		"tcti_a64_generated_system_accessors[2014]"));
	CHECK(!stream_contains(first, "u64 reconciliation_identity;"));
	CHECK(!stream_contains(first,
		"u64 selector_identity; u64 condition_identity;"));
	CHECK(!stream_contains(first,
		"u32 condition_source_offset; u32 condition_source_length;"));
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
	enum tcti_a64_kbuild_system_accessor_disposition saved_disposition;
	const char *saved_canonical;
	const char *saved_condition;
	const char *saved_generic_leaf;
	const char *saved_registers_sha256;
	uint32_t saved_accessor_index;
	uint32_t saved_source_length;
	uint32_t saved_condition_length;
	uint64_t saved_identity;
	FILE *output;

	CHECK(!fixture_init(&fixture));
	output = tmpfile();
	CHECK(output != NULL);
	CHECK(tcti_a64_kbuild_generate_header(
		      &fixture.metadata, fixture.source,
		      TCTI_A64_KBUILD_SOURCE_COUNT - 1U,
		      fixture.classification, TCTI_A64_KBUILD_SOURCE_COUNT,
		      &fixture.accessor_metadata, fixture.accessors,
		      TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT,
		      output) == TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH);
	CHECK(stream_length(output) == 0);
	fclose(output);

	output = tmpfile();
	CHECK(output != NULL);
	CHECK(tcti_a64_kbuild_generate_header(
		      &fixture.metadata, fixture.source,
		      TCTI_A64_KBUILD_SOURCE_COUNT, fixture.classification,
		      TCTI_A64_KBUILD_SOURCE_COUNT,
		      &fixture.accessor_metadata, fixture.accessors,
		      TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT - 1U,
		      output) ==
	      TCTI_A64_KBUILD_GENERATOR_ACCESSOR_COUNT_MISMATCH);
	CHECK(stream_length(output) == 0);
	fclose(output);

	fixture.metadata.instructions_sha256 =
		"b1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe";
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH));
	fixture.metadata.instructions_sha256 =
		TCTI_A64_KBUILD_INSTRUCTIONS_SHA256;

	fixture.metadata.timestamp = "2026-06-24 17:12:15";
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH));
	fixture.metadata.timestamp = TCTI_A64_KBUILD_INSTRUCTIONS_TIMESTAMP;

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

	saved_condition = fixture.source[3].condition_tcnd_hex;
	fixture.source[3].condition_tcnd_hex = "not-tcnd";
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW));
	fixture.source[3].condition_tcnd_hex = saved_condition;

	saved_source_length = fixture.source[4].source_length;
	fixture.source[4].source_length = 0U;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW));
	fixture.source[4].source_length = saved_source_length;

	saved_registers_sha256 = fixture.accessor_metadata.registers_sha256;
	fixture.accessor_metadata.registers_sha256 =
		"6bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874";
	CHECK(!expect_failure(
		&fixture, TCTI_A64_KBUILD_GENERATOR_ACCESSOR_METADATA_MISMATCH));
	fixture.accessor_metadata.registers_sha256 = saved_registers_sha256;

#define EXPECT_ACCESSOR_BLOCKER(field) do { \
	fixture.accessor_metadata.mapped_count--; \
	fixture.accessor_metadata.field = 1U; \
	CHECK(!expect_failure(&fixture, \
		TCTI_A64_KBUILD_GENERATOR_ACCESSOR_BLOCKER)); \
	fixture.accessor_metadata.field = 0U; \
	fixture.accessor_metadata.mapped_count++; \
} while (0)
	EXPECT_ACCESSOR_BLOCKER(reserved_count);
	EXPECT_ACCESSOR_BLOCKER(privileged_count);
	EXPECT_ACCESSOR_BLOCKER(unsupported_count);
	EXPECT_ACCESSOR_BLOCKER(ambiguous_count);
	EXPECT_ACCESSOR_BLOCKER(contradictory_count);
	EXPECT_ACCESSOR_BLOCKER(invalid_count);
#undef EXPECT_ACCESSOR_BLOCKER

	saved_generic_leaf = fixture.accessors[0].generic_leaf;
	fixture.accessors[0].generic_leaf = "missing_leaf";
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].generic_leaf = saved_generic_leaf;

	saved_identity = fixture.accessors[0].selector_identity;
	fixture.accessors[0].selector_identity = 0U;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].selector_identity = saved_identity;

	saved_identity = fixture.accessors[0].condition_identity;
	fixture.accessors[0].condition_identity = 0U;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].condition_identity = saved_identity;

	saved_condition_length = fixture.accessors[0].condition_source_length;
	fixture.accessors[0].condition_source_length = 0U;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].condition_source_length = saved_condition_length;

	saved_identity = fixture.accessor_metadata.reconciliation_identity;
	fixture.accessor_metadata.reconciliation_identity ^= UINT64_C(1);
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_ACCESSOR_METADATA_MISMATCH));
	fixture.accessor_metadata.reconciliation_identity = saved_identity;

	saved_accessor_index = fixture.accessors[1].accessor_index;
	fixture.accessors[1].accessor_index = fixture.accessors[0].accessor_index;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_DUPLICATE_ACCESSOR));
	fixture.accessors[1].accessor_index = saved_accessor_index;

	saved_disposition = fixture.accessors[0].disposition;
	fixture.accessors[0].disposition =
		TCTI_A64_KBUILD_SYSTEM_ACCESSOR_RESERVED;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].disposition =
		TCTI_A64_KBUILD_SYSTEM_ACCESSOR_PRIVILEGED;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].disposition =
		TCTI_A64_KBUILD_SYSTEM_ACCESSOR_UNSUPPORTED;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].disposition =
		TCTI_A64_KBUILD_SYSTEM_ACCESSOR_AMBIGUOUS;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].disposition =
		TCTI_A64_KBUILD_SYSTEM_ACCESSOR_CONTRADICTORY;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].disposition =
		TCTI_A64_KBUILD_SYSTEM_ACCESSOR_INVALID;
	CHECK(!expect_failure(&fixture,
		TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW));
	fixture.accessors[0].disposition = saved_disposition;

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
