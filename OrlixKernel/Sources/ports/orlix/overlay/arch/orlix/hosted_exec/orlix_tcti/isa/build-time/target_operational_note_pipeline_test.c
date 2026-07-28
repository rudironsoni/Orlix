// SPDX-License-Identifier: GPL-2.0-only
#include "target_instruction_artifact_generator.h"
#include "target_inventory_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long file_length;
	char *bytes;

	if (!file || fseek(file, 0, SEEK_END) ||
	    (file_length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	bytes = malloc((size_t)file_length + 1U);
	if (!bytes || fread(bytes, 1, (size_t)file_length, file) !=
			(size_t)file_length || fclose(file)) {
		free(bytes);
		return NULL;
	}
	bytes[file_length] = '\0';
	*length = (size_t)file_length;
	return bytes;
}

static char *find_in_span(char *source, size_t offset, size_t length,
			  const char *needle, size_t needle_length)
{
	size_t index;

	if (needle_length > length)
		return NULL;
	for (index = 0; index <= length - needle_length; index++) {
		if (!memcmp(source + offset + index, needle, needle_length))
			return source + offset + index;
	}
	return NULL;
}

int main(int argc, char **argv)
{
	static const char marker[] = "\"operational_note\": null";
	static const char replacement[] =
		"\"operational_note\": \"Preserve ordering\"";
	char *source = NULL;
	char *mutated = NULL;
	char *location;
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error import_error;
	size_t leaf_index;
	size_t source_length;
	size_t prefix_length;
	size_t mutated_length;
	char digest[65];
	FILE *mutated_output = NULL;
	FILE *artifact_output = NULL;
	int status = EXIT_FAILURE;

	if (argc != 4)
		return EXIT_FAILURE;
	source = read_file(argv[1], &source_length);
	if (!source)
		goto out;
	if (orlix_tcti_target_inventory_import(source, source_length, &inventory,
					      &import_error))
		goto out;
	location = NULL;
	for (leaf_index = 0; leaf_index < inventory.leaf_count; leaf_index++) {
		const struct orlix_tcti_target_leaf *leaf =
			&inventory.leaves[leaf_index];

		if (leaf->operational_note_state !=
		    ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_ABSENT)
			continue;
		location = find_in_span(source, leaf->source_offset,
					leaf->source_length, marker,
					sizeof(marker) - 1U);
		if (location)
			break;
	}
	if (!location)
		goto out;
	orlix_tcti_target_inventory_destroy(&inventory);
	prefix_length = (size_t)(location - source);
	mutated_length = source_length - (sizeof(marker) - 1U) +
		(sizeof(replacement) - 1U);
	mutated = malloc(mutated_length + 1U);
	if (!mutated)
		goto out;
	memcpy(mutated, source, prefix_length);
	memcpy(mutated + prefix_length, replacement, sizeof(replacement) - 1U);
	memcpy(mutated + prefix_length + sizeof(replacement) - 1U,
	       location + sizeof(marker) - 1U,
	       source_length - prefix_length - (sizeof(marker) - 1U));
	mutated[mutated_length] = '\0';
	orlix_tcti_target_inventory_sha256(mutated, mutated_length, digest);
	mutated_output = fopen(argv[2], "wb");
	artifact_output = fopen(argv[3], "wb");
	if (!mutated_output || !artifact_output ||
	    fwrite(mutated, 1, mutated_length, mutated_output) != mutated_length ||
	    fclose(mutated_output))
		goto out;
	mutated_output = NULL;
	if (orlix_tcti_target_instruction_artifact_emit_expected(
		    mutated, mutated_length, digest, artifact_output) !=
	    ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK ||
	    fclose(artifact_output))
		goto out;
	artifact_output = NULL;
	status = EXIT_SUCCESS;
out:
	orlix_tcti_target_inventory_destroy(&inventory);
	if (mutated_output)
		fclose(mutated_output);
	if (artifact_output)
		fclose(artifact_output);
	free(mutated);
	free(source);
	return status;
}
