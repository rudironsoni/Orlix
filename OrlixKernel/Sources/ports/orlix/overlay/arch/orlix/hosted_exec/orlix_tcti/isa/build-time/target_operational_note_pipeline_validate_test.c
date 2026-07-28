// SPDX-License-Identifier: GPL-2.0-only
#include "target_inventory_import.h"
#include "target_instruction_artifact.h"
#include "target_proof_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		goto out; \
	} \
} while (0)

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
	bytes = malloc((size_t)file_length);
	if (!bytes || fread(bytes, 1, (size_t)file_length, file) !=
			(size_t)file_length || fclose(file)) {
		free(bytes);
		return NULL;
	}
	*length = (size_t)file_length;
	return bytes;
}

int main(int argc, char **argv)
{
	const struct orlix_tcti_target_instruction_artifact *artifact;
	const struct orlix_tcti_target_instruction_artifact_operational_note *note;
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	struct orlix_tcti_target_operational_note_mapping_result mapping_result;
	static const char wrong_digest[] =
		"1111111111111111111111111111111111111111111111111111111111111111";
	struct orlix_tcti_target_proof_case proof_case = {
		.name = "operational_note_case",
		.obligations = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE,
	};
	struct orlix_tcti_target_proof_binding proof_binding = {
		.kunit_case_mask = ORLIX_TCTI_PROOF_U64_C(1),
	};
	struct orlix_tcti_target_proof_registry_entry registry = {
		.id = "operational-note-proof",
		.obligations = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE,
		.kunit_cases = &proof_case,
		.kunit_case_count = 1U,
		.bindings = &proof_binding,
		.binding_count = 1U,
	};
	struct orlix_tcti_target_operational_note_proof_mapping mapping = {
		.proof_id = "operational-note-proof",
		.kunit_case_name = "operational_note_case",
	};
	char *source = NULL;
	size_t source_length;
	char digest[65];
	int status = EXIT_FAILURE;

	if (argc != 2)
		return EXIT_FAILURE;
	source = read_file(argv[1], &source_length);
	CHECK(source != NULL);
	orlix_tcti_target_inventory_sha256(source, source_length, digest);
	artifact = orlix_tcti_target_instruction_artifact_canonical();
	CHECK(artifact != NULL);
	CHECK(artifact->version == 5U);
	CHECK(artifact->operational_note_count == 1U);
	CHECK(!orlix_tcti_target_instruction_artifact_validate_expected(
		artifact, digest, &validation));
	note = &artifact->operational_notes[0];
	proof_binding.source_ordinal = note->leaf_index;
	mapping.leaf_index = note->leaf_index;
	mapping.source_identity = (const char *)artifact->string_pool +
		note->source_identity_offset;
	mapping.source_sha256 = (const char *)artifact->string_pool +
		note->source_sha256_offset;
	CHECK(!orlix_tcti_target_operational_note_proof_mappings_validate(
		artifact, &mapping, 1U, &registry, 1U, &mapping_result));
	CHECK(mapping_result.mapped_count == 1U);
	mapping.source_sha256 = wrong_digest;
	CHECK(orlix_tcti_target_operational_note_proof_mappings_validate(
		artifact, &mapping, 1U, &registry, 1U, &mapping_result) == -1);
	CHECK(mapping_result.error ==
		ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_DIGEST_MISMATCH);
	mapping.source_sha256 = (const char *)artifact->string_pool +
		note->source_sha256_offset;
	proof_binding.source_ordinal++;
	CHECK(orlix_tcti_target_operational_note_proof_mappings_validate(
		artifact, &mapping, 1U, &registry, 1U, &mapping_result) == -1);
	CHECK(mapping_result.error ==
	      ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_BINDING_MISMATCH);
	status = EXIT_SUCCESS;
out:
	free(source);
	if (status == EXIT_SUCCESS)
		puts("operational note production pipeline: passed");
	return status;
}
