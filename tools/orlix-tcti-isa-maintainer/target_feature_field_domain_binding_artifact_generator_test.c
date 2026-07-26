/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding_artifact_generator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static char *read_file(const char *path, size_t *length)
{
	FILE *file;
	long file_length;
	char *data;

	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) || (file_length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	data = malloc((size_t)file_length + 1U);
	if (!data || fread(data, 1, (size_t)file_length, file) !=
		    (size_t)file_length || fclose(file)) {
		free(data);
		return NULL;
	}
	data[file_length] = '\0';
	*length = (size_t)file_length;
	return data;
}

static int artifact_text(FILE *file, char **text)
{
	long length;

	if (fflush(file) || fseek(file, 0, SEEK_END) ||
	    (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET))
		return -1;
	*text = malloc((size_t)length + 1U);
	if (!*text || fread(*text, 1, (size_t)length, file) != (size_t)length)
		return -1;
	(*text)[length] = '\0';
	return 0;
}

static size_t occurrences(const char *text, const char *needle)
{
	size_t count = 0;
	const char *where = text;

	while ((where = strstr(where, needle)) != NULL) {
		count++;
		where += strlen(needle);
	}
	return count;
}

static int pinned_census_and_determinism(const char *features_path,
	const char *registers_path)
{
	char *features;
	char *registers;
	size_t feature_length;
	size_t register_length;
	FILE *first = tmpfile();
	FILE *second = tmpfile();
	char *first_text = NULL;
	char *second_text = NULL;
	struct orlix_tcti_feature_model feature_model = { 0 };
	struct orlix_tcti_register_model register_model = { 0 };
	struct orlix_tcti_feature_error feature_error = { 0 };
	struct orlix_tcti_register_model_error register_error = { 0 };
	struct orlix_tcti_feature_field_domain_bindings bindings = { 0 };
	struct orlix_tcti_feature_field_domain_binding_error binding_error = { 0 };
	struct orlix_tcti_feature_field_domain_binding *mapped = NULL;
	struct orlix_tcti_feature_field_domain_binding *ambiguous = NULL;
	size_t index;

	CHECK(first && second);
	features = read_file(features_path, &feature_length);
	registers = read_file(registers_path, &register_length);
	CHECK(features && registers);
	CHECK(!orlix_tcti_target_feature_model_import(features, feature_length,
		&feature_model, &feature_error));
	CHECK(!orlix_tcti_register_model_import(registers, register_length,
		&register_model, &register_error));
	CHECK(!orlix_tcti_target_feature_field_domain_bindings_build(&feature_model,
		&register_model, &bindings, &binding_error));
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK);
	for (index = 0; index < bindings.count; index++) {
		if (!mapped && bindings.items[index].disposition ==
			    ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED)
			mapped = &bindings.items[index];
		if (!ambiguous && bindings.items[index].disposition ==
			    ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD)
			ambiguous = &bindings.items[index];
	}
	CHECK(mapped && ambiguous);
	CHECK(ambiguous->field_index == UINT32_MAX);
	CHECK(ambiguous->value_relation_index == UINT32_MAX);
	CHECK(!ambiguous->field_source_offset &&
	      !ambiguous->field_source_length &&
	      !ambiguous->value_relation_source_offset &&
	      !ambiguous->value_relation_source_length);
	ambiguous->field_index = 0U;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	ambiguous->field_index = UINT32_MAX;
	ambiguous->field_source_length = 1U;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	ambiguous->field_source_length = 0U;
	mapped->value_relation_index = UINT32_MAX;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->value_relation_index = mapped->field_index;
	mapped->field_source_length++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->field_source_length--;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK);
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_emit_model(
		&feature_model, feature_length / 2U, &register_model, register_length,
		first) == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_SOURCE_SPAN);
	CHECK(!fseek(first, 0, SEEK_SET));
	CHECK(fgetc(first) == EOF);
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	orlix_tcti_register_model_destroy(&register_model);
	orlix_tcti_target_feature_model_destroy(&feature_model);
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_emit(features,
		feature_length, registers, register_length, first) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK);
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_emit(features,
		feature_length, registers, register_length, second) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK);
	CHECK(!artifact_text(first, &first_text));
	CHECK(!artifact_text(second, &second_text));
	CHECK(!strcmp(first_text, second_text));
	CHECK(strstr(first_text,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS(605U, 362U, 604U, 1U)") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(UINT64_C(") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE(0U,") != NULL);
	CHECK(occurrences(first_text,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE(") == 605U);
	CHECK(strstr(first_text, ", 4U,") != NULL);
	free(first_text);
	free(second_text);
	free(features);
	free(registers);
	fclose(first);
	fclose(second);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s Features.json Registers.json\\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (orlix_tcti_target_feature_field_domain_binding_artifact_emit(NULL, 0,
		"x", 1U, stdout) !=
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_ARGUMENT)
		return EXIT_FAILURE;
	return pinned_census_and_determinism(argv[1], argv[2]) ?
		EXIT_FAILURE : EXIT_SUCCESS;
}
