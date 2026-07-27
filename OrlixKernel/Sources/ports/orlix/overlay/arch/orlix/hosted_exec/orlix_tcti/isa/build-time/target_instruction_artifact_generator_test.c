/* SPDX-License-Identifier: GPL-2.0-only */
/* Focused host proof for the JSON-to-C Instructions artifact boundary. */
#define TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MAIN
#include "target_instruction_artifact_generator.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
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
	if (!data || fread(data, 1, (size_t)file_length, file) != (size_t)file_length ||
	    fclose(file)) {
		free(data);
		return NULL;
	}
	data[file_length] = '\0';
	*length = (size_t)file_length;
	return data;
}

static char *read_stream(FILE *file, size_t *length)
{
	long file_length;
	char *data;

	if (fflush(file) || fseek(file, 0, SEEK_END) ||
	    (file_length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET))
		return NULL;
	data = malloc((size_t)file_length + 1U);
	if (!data || fread(data, 1, (size_t)file_length, file) != (size_t)file_length) {
		free(data);
		return NULL;
	}
	data[file_length] = '\0';
	*length = (size_t)file_length;
	return data;
}

static int emit_is_empty_after_failure(const char *source, size_t length,
	enum orlix_tcti_target_instruction_artifact_error expected)
{
	FILE *output = tmpfile();

	CHECK(output != NULL);
	CHECK(orlix_tcti_target_instruction_artifact_emit(source, length, output) == expected);
	CHECK(fflush(output) == 0);
	CHECK(ftell(output) == 0);
	fclose(output);
	return 0;
}

static char *replace_once(const char *source, size_t source_length,
	const char *from, const char *to, size_t *length)
{
	const char *location = strstr(source, from);
	size_t before;
	size_t from_length = strlen(from);
	size_t to_length = strlen(to);
	char *copy;

	if (!location)
		return NULL;
	before = (size_t)(location - source);
	copy = malloc(source_length - from_length + to_length + 1U);
	if (!copy)
		return NULL;
	memcpy(copy, source, before);
	memcpy(copy + before, to, to_length);
	memcpy(copy + before + to_length, location + from_length,
	       source_length - before - from_length);
	*length = source_length - from_length + to_length;
	copy[*length] = '\0';
	return copy;
}

static int model_is_lossless(const char *source, size_t source_length)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error import_error = { 0 };
	struct artifact_model model = { 0 };
	enum orlix_tcti_target_instruction_artifact_error error;
	size_t index;

	CHECK(orlix_tcti_target_inventory_import(source, source_length, &inventory,
					 &import_error) == 0);
	CHECK(inventory.leaf_count == ORLIX_TCTI_A64_TARGET_LEAF_COUNT);
	error = build_model(&inventory, &model);
	CHECK(error == ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OK);
	CHECK(model.operand_count == inventory.operand_count);
	CHECK(model.fixed_operand_count == inventory.fixed_operand_count);
	CHECK(model.fixed_operand_count == 16675U);
	CHECK(model.strings.length != 0);
	CHECK(model.conditions.length != 0);
	for (index = 0; index < inventory.leaf_count; index++) {
		const struct orlix_tcti_target_leaf *source_leaf = &inventory.leaves[index];
		const struct artifact_leaf *leaf = &model.leaves[index];
		size_t operand;

		CHECK(leaf->name_offset < model.strings.length);
		CHECK(leaf->mnemonic_offset < model.strings.length);
		CHECK(leaf->operation_offset < model.strings.length);
		CHECK(!strcmp((char *)model.strings.data + leaf->name_offset,
			      source_leaf->name));
		CHECK(!strcmp((char *)model.strings.data + leaf->mnemonic_offset,
			      source_leaf->mnemonic));
		CHECK(!strcmp((char *)model.strings.data + leaf->operation_offset,
			      source_leaf->operation_id));
		CHECK(leaf->encoding_mask == source_leaf->encoding_mask);
		CHECK(leaf->encoding_pattern == source_leaf->encoding_pattern);
		CHECK(leaf->condition_offset <= model.conditions.length);
		CHECK(leaf->condition_length <= model.conditions.length - leaf->condition_offset);
		CHECK(leaf->condition_length >= 5U);
		CHECK(!memcmp(model.conditions.data + leaf->condition_offset, "TCND\1", 5U));
		CHECK(leaf->operand_first <= model.operand_count);
		CHECK(leaf->operand_count <= model.operand_count - leaf->operand_first);
		CHECK(leaf->fixed_operand_first <= model.fixed_operand_count);
		CHECK(leaf->fixed_operand_count <=
			model.fixed_operand_count - leaf->fixed_operand_first);
		for (operand = leaf->operand_first;
		     operand < (size_t)leaf->operand_first + leaf->operand_count;
		     operand++) {
			const struct artifact_operand *entry = &model.operands[operand];

			CHECK(entry->leaf_index == index);
			CHECK(entry->name_offset < model.strings.length);
			CHECK(entry->condition_offset <= model.conditions.length);
			CHECK(entry->condition_length <=
			      model.conditions.length - entry->condition_offset);
			CHECK(entry->condition_length >= 5U);
			CHECK(!memcmp(model.conditions.data + entry->condition_offset,
				      "TCND\1", 5U));
		}
		for (operand = leaf->fixed_operand_first;
		     operand < (size_t)leaf->fixed_operand_first +
			leaf->fixed_operand_count; operand++) {
			const struct artifact_fixed_operand *entry =
				&model.fixed_operands[operand];
			const struct orlix_tcti_target_fixed_operand *source_entry = NULL;
			size_t source_index;
			char source_identity[sizeof(source_sha256) + 2U + 20U + 20U];

			for (source_index = 0;
			     source_index < inventory.fixed_operand_count; source_index++)
				if (inventory.fixed_operands[source_index].leaf_index == index &&
				    !strcmp(inventory.fixed_operands[source_index].name,
					(char *)model.strings.data + entry->name_offset)) {
					source_entry = &inventory.fixed_operands[source_index];
					break;
				}
			CHECK(source_entry != NULL);
			CHECK(entry->leaf_index == index);
			CHECK(entry->condition_offset ==
				model.condition_map[source_entry->condition].offset);
			CHECK(entry->condition_length ==
				model.condition_map[source_entry->condition].length);
			CHECK(entry->fixed_mask == source_entry->fixed_mask);
			CHECK(entry->fixed_value == source_entry->fixed_value);
			CHECK(entry->start == source_entry->start);
			CHECK(entry->width == source_entry->width);
			CHECK(entry->source_offset == source_entry->source_offset);
			CHECK(entry->source_length == source_entry->source_length);
			CHECK(snprintf(source_identity, sizeof(source_identity), "%s:%zu:%zu",
				source_sha256, source_entry->source_offset,
				source_entry->source_length) > 0);
			CHECK(!strcmp((char *)model.strings.data +
				entry->source_identity_offset, source_identity));
		}
	}
	CHECK(model.instruction_alias_count ==
		ORLIX_TCTI_A64_TARGET_INSTRUCTION_ALIAS_COUNT);
	CHECK(model.operation_alias_count ==
		ORLIX_TCTI_A64_TARGET_REACHABLE_OPERATION_ALIAS_COUNT);
	{
		static const struct {
			uint32_t ordinal;
			const char *name;
			uint8_t start;
			uint8_t width;
			uint32_t value;
		} expected[] = {
			{ 203U, "opc", 22U, 2U, 0U },
			{ 204U, "opc", 22U, 2U, 0x00400000U },
			{ 205U, "opc", 22U, 2U, 0x00800000U },
			{ 2169U, "op21", 29U, 2U, 0U },
			{ 2170U, "op21", 29U, 2U, 0U },
		};
		size_t expected_index;

		for (expected_index = 0; expected_index <
		     sizeof(expected) / sizeof(expected[0]); expected_index++) {
			const struct artifact_leaf *leaf =
				&model.leaves[expected[expected_index].ordinal];
			const struct artifact_fixed_operand *entry = NULL;
			size_t fixed_index;

			for (fixed_index = leaf->fixed_operand_first;
			     fixed_index < (size_t)leaf->fixed_operand_first +
				leaf->fixed_operand_count; fixed_index++) {
				const struct artifact_fixed_operand *candidate =
					&model.fixed_operands[fixed_index];

				if (!strcmp((char *)model.strings.data +
					candidate->name_offset, expected[expected_index].name)) {
					entry = candidate;
					break;
				}
			}
			CHECK(entry != NULL);
			CHECK(entry->leaf_index == expected[expected_index].ordinal);
			CHECK(entry->start == expected[expected_index].start);
			CHECK(entry->width == expected[expected_index].width);
			CHECK(entry->fixed_mask == ((UINT32_C(3) << entry->start)));
			CHECK(entry->fixed_value == expected[expected_index].value);
			CHECK(entry->source_length != 0U);
			CHECK(entry->source_identity_offset < model.strings.length);
		}
	}
	for (index = 0; index < inventory.instruction_alias_count; index++) {
		const struct orlix_tcti_target_instruction_alias *source_alias =
			&inventory.instruction_aliases[index];
		const struct artifact_instruction_alias *alias =
			&model.instruction_aliases[index];
		char source_identity[sizeof(source_sha256) + 2U + 20U + 20U];

		CHECK(alias->ordinal == source_alias->ordinal);
		CHECK(!strcmp((char *)model.strings.data + alias->name_offset,
			source_alias->name));
		CHECK(!strcmp((char *)model.strings.data +
			alias->declared_operation_offset, source_alias->operation_id));
		CHECK(!strcmp((char *)model.strings.data +
			alias->resolved_operation_offset,
			source_alias->canonical_operation_id));
		CHECK(alias->condition_offset ==
			model.condition_map[source_alias->condition].offset);
		CHECK(alias->condition_length ==
			model.condition_map[source_alias->condition].length);
		CHECK(!memcmp(model.conditions.data + alias->condition_offset, "TCND\1", 5U));
		CHECK(alias->source_offset == source_alias->source_offset);
		CHECK(alias->source_length == source_alias->source_length);
		CHECK(alias->condition_source_offset == source_alias->condition_source_offset);
		CHECK(alias->condition_source_length == source_alias->condition_source_length);
		CHECK(alias->preferred_source_offset == source_alias->preferred_source_offset);
		CHECK(alias->preferred_source_length == source_alias->preferred_source_length);
		CHECK(alias->preferred_present == source_alias->preferred_present);
		CHECK(snprintf(source_identity, sizeof(source_identity), "%s:%zu:%zu",
			source_sha256, source_alias->source_offset,
			source_alias->source_length) > 0);
		CHECK(!strcmp((char *)model.strings.data + alias->source_identity_offset,
			source_identity));
	}
	{
		size_t operation_alias_index = 0;

		for (index = 0; index < inventory.operation_count; index++) {
			const struct orlix_tcti_target_operation *source_operation =
				&inventory.operations[index];
			const struct artifact_operation_alias *alias;

			if (!source_operation->is_alias ||
			    !source_operation->canonical_operation_id)
				continue;
			CHECK(operation_alias_index < model.operation_alias_count);
			alias = &model.operation_aliases[operation_alias_index];
			CHECK(!strcmp((char *)model.strings.data +
				alias->declared_operation_offset, source_operation->id));
			CHECK(!strcmp((char *)model.strings.data +
				alias->target_operation_offset,
				source_operation->alias_operation_id));
			CHECK(!strcmp((char *)model.strings.data +
				alias->resolved_operation_offset,
				source_operation->canonical_operation_id));
			CHECK(alias->source_offset == source_operation->source_offset);
			CHECK(alias->source_length == source_operation->source_length);
			operation_alias_index++;
		}
		CHECK(operation_alias_index == model.operation_alias_count);
	}
	artifact_model_destroy(&model);
	orlix_tcti_target_inventory_destroy(&inventory);
	return 0;
}

static int deterministic_emission(const char *source, size_t source_length)
{
	FILE *first = tmpfile();
	FILE *second = tmpfile();
	char *first_bytes;
	char *second_bytes;
	size_t first_length;
	size_t second_length;

	CHECK(first != NULL && second != NULL);
	CHECK(orlix_tcti_target_instruction_artifact_emit(source, source_length, first) ==
	      ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OK);
	CHECK(orlix_tcti_target_instruction_artifact_emit(source, source_length, second) ==
	      ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OK);
	first_bytes = read_stream(first, &first_length);
	second_bytes = read_stream(second, &second_length);
	CHECK(first_bytes != NULL && second_bytes != NULL);
	CHECK(first_length == second_length);
	CHECK(!memcmp(first_bytes, second_bytes, first_length));
	CHECK(strstr(first_bytes, "ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT 4350U") != NULL);
	CHECK(strstr(first_bytes, source_sha256) != NULL);
	CHECK(strstr(first_bytes, "#include \"target_instruction_artifact.h\"") != NULL);
	CHECK(strstr(first_bytes,
		"static const struct orlix_tcti_target_instruction_artifact orlix_tcti_a64_instruction_artifact") != NULL);
	CHECK(strstr(first_bytes, "orlix_tcti_a64_instruction_artifact_condition_pool") != NULL);
	free(first_bytes);
	free(second_bytes);
	fclose(first);
	fclose(second);
	return 0;
}

static int no_partial_output_contract(const char *source, size_t source_length)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error import_error = { 0 };
	struct artifact_bytes generated = { 0 };
	char *mutation;
	size_t mutation_length;
	size_t original_operand_count;
	enum orlix_tcti_target_instruction_artifact_error artifact_error;

	CHECK(emit_is_empty_after_failure("{", 1U,
		ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE) == 0);
	CHECK(emit_is_empty_after_failure("\\u0000", 6U,
		ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE) == 0);
	{
		static const char raw_nul[] = { '{', '\0', '}' };

		CHECK(emit_is_empty_after_failure(raw_nul, sizeof(raw_nul),
			ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE) == 0);
	}
	mutation = replace_once(source, source_length, "ADD_32_addsub_imm",
				"BDD_32_addsub_imm", &mutation_length);
	CHECK(mutation != NULL);
	CHECK(emit_is_empty_after_failure(mutation, mutation_length,
		ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_DIGEST) == 0);
	free(mutation);

	CHECK(orlix_tcti_target_inventory_import(source, source_length, &inventory,
					 &import_error) == 0);
	inventory.leaf_count = ORLIX_TCTI_A64_TARGET_LEAF_COUNT - 1U;
	CHECK(generate_from_inventory(&inventory, &generated) ==
		ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT);
	CHECK(generated.data == NULL && generated.length == 0);
	inventory.leaf_count = ORLIX_TCTI_A64_TARGET_LEAF_COUNT;
	original_operand_count = inventory.operand_count;
	inventory.operand_count = (size_t)UINT32_MAX + 1U;
	artifact_error = generate_from_inventory(&inventory, &generated);
	/* destroy walks the imported operand allocation, not a synthetic count. */
	inventory.operand_count = original_operand_count;
	CHECK(artifact_error == ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW);
	CHECK(generated.data == NULL && generated.length == 0);
	orlix_tcti_target_inventory_destroy(&inventory);
	return 0;
}

int main(int argc, char **argv)
{
	char *source;
	size_t source_length;

	if (argc != 2) {
		fprintf(stderr, "usage: %s Instructions.json\n", argv[0]);
		return 2;
	}
	source = read_file(argv[1], &source_length);
	if (!source) {
		perror(argv[1]);
		return 1;
	}
	if (model_is_lossless(source, source_length) ||
	    deterministic_emission(source, source_length) ||
	    no_partial_output_contract(source, source_length)) {
		free(source);
		return 1;
	}
	free(source);
	puts("PASS target instruction artifact generator lossless and no-partial-output checks");
	return 0;
}
