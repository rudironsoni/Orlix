/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_asl_availability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t token_count(const char *text, const char *token)
{
	size_t count = 0;
	size_t token_length = strlen(token);

	while ((text = strstr(text, token)) != NULL) {
		count++;
		text += token_length;
	}
	return count;
}

static char *read_file(const char *path, size_t *length)
{
	FILE *input = fopen(path, "rb");
	long size;
	char *data;

	if (!input || fseek(input, 0, SEEK_END) || (size = ftell(input)) < 0 ||
	    fseek(input, 0, SEEK_SET)) {
		if (input)
			fclose(input);
		return NULL;
	}
	data = malloc((size_t)size + 1U);
	if (!data || fread(data, 1, (size_t)size, input) != (size_t)size ||
	    fclose(input)) {
		free(data);
		return NULL;
	}
	data[size] = '\0';
	*length = (size_t)size;
	return data;
}

static char *capture_artifact(
	const char *source, size_t length,
	const struct orlix_tcti_arm_xml_package *package)
{
	FILE *output = tmpfile();
	long size;
	char *artifact;

	if (!output ||
	    orlix_tcti_target_semantic_provenance_emit(source, length, package,
						       output) !=
		ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_OK ||
	    fflush(output) || fseek(output, 0, SEEK_END) ||
	    (size = ftell(output)) < 0 || fseek(output, 0, SEEK_SET)) {
		if (output)
			fclose(output);
		return NULL;
	}
	artifact = malloc((size_t)size + 1U);
	if (!artifact ||
	    fread(artifact, 1, (size_t)size, output) != (size_t)size) {
		free(artifact);
		fclose(output);
		return NULL;
	}
	artifact[size] = '\0';
	fclose(output);
	return artifact;
}

int main(int argc, char **argv)
{
	struct orlix_tcti_arm_xml_package package = { 0 };
	char *source;
	char *artifact;
	size_t source_length;
	int status = 1;

	if (argc != 4)
		return 2;
	if (orlix_tcti_arm_xml_package_validate(argv[2], argv[3], &package) !=
	    ORLIX_TCTI_ARM_XML_PACKAGE_OK)
		return 1;
	source = read_file(argv[1], &source_length);
	artifact = source ? capture_artifact(source, source_length, &package) : NULL;
	if (!artifact)
		goto out;

	if (token_count(artifact,
			"ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(") != 1U ||
	    token_count(artifact,
			"ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(") != 4332U ||
	    token_count(artifact,
			"ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(") != 18U ||
	    !strstr(artifact, "authority=Arm_DDI0602_2026_06") ||
	    !strstr(artifact, "distribution=external_non_redistributed") ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_ARCHIVE_SHA256) ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_RELEASE_DIGEST) ||
	    !strstr(artifact, ORLIX_TCTI_ARM_XML_SHARED_SHA256) ||
	    !strstr(artifact,
		     "ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(2235U, "
		     "\"TENTER_te_exception\", \"Instructions.json#operations/TENTER/"
		     "operation\"") ||
	    !strstr(artifact,
		     "ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(2686U, "
		     "\"SETGOETN_memset_go\"") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_AVAILABILITY") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_BODY_") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_DECODE_") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_CORPUS_") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_HELPERS_"))
		goto out;

	puts("PASS external DDI0602 semantic provenance and typed unspecified rows");
	status = 0;
out:
	free(artifact);
	free(source);
	orlix_tcti_arm_xml_package_destroy(&package);
	return status;
}
