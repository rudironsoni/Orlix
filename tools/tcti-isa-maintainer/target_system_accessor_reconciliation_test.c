// SPDX-License-Identifier: GPL-2.0-only
#include "target_register_model.h"
#include "target_system_accessor_reconciliation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int synthetic_mapping_and_fail_closed_dispositions(void)
{
	static const struct tcti_register_accessor accessors[] = {
		{ .type = "Accessors.SystemAccessor", .name = "A64.MRS",
		  .first_system_encoding = 0U, .system_encoding_count = 1U,
		  .condition_expression = 7U, .source_offset = 10U,
		  .source_length = 11U, .condition_offset = 9U,
		  .condition_length = 9U },
		{ .type = "Accessors.SystemAccessor", .name = "A64.Unknown",
		  .first_system_encoding = 1U, .system_encoding_count = 1U },
		{ .type = "Accessors.SystemAccessor", .name = "A64.MSRregister",
		  .first_system_encoding = 2U, .system_encoding_count = 2U },
		{ .type = "Accessors.SystemAccessor", .name = "A64.SYS",
		  .first_system_encoding = UINT32_MAX, .system_encoding_count = 0U },
	};
	static const struct tcti_register_system_encoding encodings[] = {
		{ .accessor_index = 0U, .source_offset = 20U, .source_length = 21U },
		{ .accessor_index = 1U },
		{ .accessor_index = 2U },
		{ .accessor_index = 2U },
	};
	static const struct tcti_register_system_selector selectors[] = {
		{ .encoding_index = 0U, .source_offset = 0U, .source_length = 8U },
		{ .encoding_index = 1U },
		{ .encoding_index = 2U }, { .encoding_index = 3U },
	};
	static const char source[] = "selector condition";
	const struct tcti_register_model model = {
		.accessors = (struct tcti_register_accessor *)accessors,
		.accessor_count = sizeof(accessors) / sizeof(accessors[0]),
		.system_encodings = (struct tcti_register_system_encoding *)encodings,
		.system_encoding_count = sizeof(encodings) / sizeof(encodings[0]),
		.system_selectors = (struct tcti_register_system_selector *)selectors,
		.system_selector_count = sizeof(selectors) / sizeof(selectors[0]),
		.source = (char *)source,
		.source_length = sizeof(source) - 1U,
		.expression_count = 8U,
	};
	struct tcti_system_accessor_reconciliation_result result = { 0 };
	FILE *artifact;
	char artifact_text[1024] = { 0 };

	CHECK(tcti_system_accessor_reconcile(&model, &result) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK);
	CHECK(result.entry_count == 4U);
	CHECK(result.census.aarch64_accessors == 4U);
	CHECK(result.census.mapped == 1U);
	CHECK(result.census.reserved == 0U);
	CHECK(result.census.privileged == 0U);
	CHECK(result.census.unsupported == 1U);
	CHECK(result.census.ambiguous == 1U);
	CHECK(result.census.contradictory == 0U);
	CHECK(result.census.invalid == 1U);
	CHECK(result.entries[0].accessor_index == 0U);
	CHECK(result.entries[0].encoding_index == 0U);
	CHECK(result.entries[0].condition_expression == 7U);
	CHECK(result.entries[0].selector_identity != 0U);
	CHECK(result.entries[0].condition_identity != 0U);
	CHECK(result.entries[0].selector_count == 1U);
	CHECK(result.entries[0].generic_leaf ==
	      TCTI_SYSTEM_ACCESSOR_LEAF_MRS_RS_SYSTEMMOVE);
	CHECK(result.entries[0].direction == TCTI_SYSTEM_ACCESSOR_DIRECTION_READ);
	CHECK(result.entries[0].accessor_source_offset == 10U);
	CHECK(result.entries[0].encoding_source_offset == 20U);
	CHECK(result.entries[1].disposition == TCTI_SYSTEM_ACCESSOR_UNSUPPORTED);
	CHECK(result.entries[2].disposition == TCTI_SYSTEM_ACCESSOR_AMBIGUOUS);
	CHECK(result.entries[3].disposition == TCTI_SYSTEM_ACCESSOR_INVALID);
	CHECK(tcti_system_accessor_reconciliation_validate(&result) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_AMBIGUOUS);
	tcti_system_accessor_reconciliation_destroy(&result);
	artifact = tmpfile();
	CHECK(artifact != NULL);
	CHECK(tcti_system_accessor_reconciliation_emit(&model, artifact) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK);
	CHECK(!fflush(artifact));
	CHECK(!fseek(artifact, 0, SEEK_SET));
	CHECK(fread(artifact_text, 1, sizeof(artifact_text) - 1U, artifact) > 0U);
	CHECK(strstr(artifact_text,
	      "TCTI_A64_SYSTEM_ACCESSOR_COUNTS(4U, 1U, 0U, 0U, 1U, 1U, "
	      "0U, 1U)") != NULL);
	CHECK(strstr(artifact_text,
	      "TCTI_A64_SYSTEM_ACCESSOR(0U, 0U, \"A64.MRS\", "
	      "\"MRS_RS_systemmove\", 1U, 0U, 1U, 7U, UINT64_C(") != NULL);
	fclose(artifact);
	return 0;
}

static int explicit_blocking_dispositions(void)
{
	struct tcti_system_accessor_reconciliation_entry entry = { 0 };
	struct tcti_system_accessor_reconciliation_result result = {
		.entries = &entry,
		.entry_count = 1U,
		.census.aarch64_accessors = 1U,
	};

	result.census.reserved = 1U;
	CHECK(tcti_system_accessor_reconciliation_validate(&result) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_RESERVED);
	result.census.reserved = 0U;
	result.census.privileged = 1U;
	CHECK(tcti_system_accessor_reconciliation_validate(&result) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_PRIVILEGED);
	result.census.privileged = 0U;
	result.census.contradictory = 1U;
	CHECK(tcti_system_accessor_reconciliation_validate(&result) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_CONTRADICTORY);
	return 0;
}

static char *read_file(const char *path, size_t *length)
{
	FILE *file;
	char *source;
	long file_length;

	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) ||
	    (file_length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	source = malloc((size_t)file_length);
	if (!source || fread(source, 1, (size_t)file_length, file) !=
		(size_t)file_length) {
		free(source);
		fclose(file);
		return NULL;
	}
	fclose(file);
	*length = (size_t)file_length;
	return source;
}

static int pinned_aarch64_census(const char *path)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error import_error = { 0 };
	struct tcti_system_accessor_reconciliation_result result = { 0 };
	char *source;
	size_t length;

	source = read_file(path, &length);
	CHECK(source != NULL);
	CHECK(!tcti_register_model_import(source, length, &model, &import_error));
	CHECK(tcti_system_accessor_reconcile(&model, &result) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK);
	CHECK(result.entry_count == 2014U);
	CHECK(result.census.aarch64_accessors == 2014U);
	CHECK(result.census.mapped == 2014U);
	CHECK(result.census.reserved == 0U);
	CHECK(result.census.privileged == 0U);
	CHECK(result.census.unsupported == 0U);
	CHECK(result.census.ambiguous == 0U);
	CHECK(result.census.contradictory == 0U);
	CHECK(result.census.invalid == 0U);
	CHECK(tcti_system_accessor_reconciliation_validate(&result) ==
	      TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK);
	{
		size_t index;
		size_t mrrs = 0;
		size_t msrr = 0;
		size_t sysp = 0;

		for (index = 0; index < result.entry_count; index++) {
			const struct tcti_system_accessor_reconciliation_entry *entry =
				&result.entries[index];

			if (!strcmp(entry->accessor_name, "A64.MRRS")) {
				CHECK(entry->generic_leaf ==
				      TCTI_SYSTEM_ACCESSOR_LEAF_MRRS_RS_SYSTEMMOVEPR);
				CHECK(entry->direction ==
				      TCTI_SYSTEM_ACCESSOR_DIRECTION_READ);
				mrrs++;
			} else if (!strcmp(entry->accessor_name, "A64.MSRRregister")) {
				CHECK(entry->generic_leaf ==
				      TCTI_SYSTEM_ACCESSOR_LEAF_MSRR_SR_SYSTEMMOVEPR);
				CHECK(entry->direction ==
				      TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE);
				msrr++;
			} else if (!strcmp(entry->accessor_name, "A64.SYSP")) {
				CHECK(entry->generic_leaf ==
				      TCTI_SYSTEM_ACCESSOR_LEAF_SYSP_CR_SYSPAIRINSTRS);
				CHECK(entry->direction ==
				      TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE);
				sysp++;
			}
		}
		CHECK(mrrs == 13U);
		CHECK(msrr == 13U);
		CHECK(sysp == 1U);
	}
	tcti_system_accessor_reconciliation_destroy(&result);
	tcti_register_model_destroy(&model);
	free(source);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "--synthetic"))
		return (synthetic_mapping_and_fail_closed_dispositions() ||
			explicit_blocking_dispositions()) ?
			EXIT_FAILURE : EXIT_SUCCESS;
	if (argc != 2) {
		fprintf(stderr, "usage: %s Registers.json\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (synthetic_mapping_and_fail_closed_dispositions() ||
	    explicit_blocking_dispositions() ||
	    pinned_aarch64_census(argv[1]))
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
