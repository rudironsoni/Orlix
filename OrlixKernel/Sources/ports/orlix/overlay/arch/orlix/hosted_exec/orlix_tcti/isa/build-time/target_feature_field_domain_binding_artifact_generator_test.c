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
	struct orlix_tcti_feature_field_domain_binding *mpam = NULL;
	size_t index;
	uint32_t original_member_kind;

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
		if (index == 292U)
			mpam = &bindings.items[index];
	}
	CHECK(mapped && mpam);
	CHECK(mpam->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED);
	CHECK(mpam->field_index == 13637U);
	CHECK(mpam->domain.fieldset_index == 1981U);
	CHECK(mpam->domain.equivalent_field_count == 2U);
	original_member_kind = mapped->domain.member_kind;
	mpam->domain.equivalent_field_count = 1U;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mpam->domain.equivalent_field_count = 2U;
	mapped->domain.register_width++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.register_width--;
	mapped->domain.field_width++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.field_width--;
	mapped->domain.type = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_COUNT;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.type = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_CONSTANT;
	mapped->domain.signedness = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_SIGNEDNESS_COUNT;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.signedness = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_UNSIGNED;
	mapped->domain.value_member_count++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.value_member_count--;
	mapped->domain.range_member_count++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.range_member_count--;
	mapped->domain.member_kind = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MEMBER_KIND_COUNT;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.member_kind = original_member_kind;
	mapped->domain.relation_value_count++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.relation_value_count--;
	mapped->domain.relation_constraint_count++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.relation_constraint_count--;
	mapped->domain.semantic_identity ^= UINT64_C(1);
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.semantic_identity ^= UINT64_C(1);
	mapped->domain.resolution_identity ^= UINT64_C(1);
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.resolution_identity ^= UINT64_C(1);
	mapped->identity_group_index++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->identity_group_index--;
	mapped->domain.fieldset_condition_length++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	mapped->domain.fieldset_condition_length--;
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
	CHECK(bindings.alternative_count == 606U);
	if (bindings.range_count) {
		bindings.ranges[0].start ^= 1U;
		CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
			&feature_model, feature_length, &register_model, register_length,
			&bindings) ==
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
		bindings.ranges[0].start ^= 1U;
	}
	if (bindings.domain_count) {
		const char *saved = bindings.domains[0].type;
		bindings.domains[0].type = "mutation";
		CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
			&feature_model, feature_length, &register_model, register_length,
			&bindings) ==
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
		bindings.domains[0].type = saved;
	}
	bindings.alternatives[0].field_source_length++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	bindings.alternatives[0].field_source_length--;
	bindings.items[0].alternative_count++;
	CHECK(orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		&feature_model, feature_length, &register_model, register_length,
		&bindings) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING);
	bindings.items[0].alternative_count--;
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
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(605U, 362U, 605U, 0U, 606U,") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(UINT64_C(") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3(0U,") != NULL);
	CHECK(occurrences(first_text,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3(") == 605U);
	CHECK(occurrences(first_text,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3(") == 606U);
	CHECK(strstr(first_text,
		"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V2(") == NULL);
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
