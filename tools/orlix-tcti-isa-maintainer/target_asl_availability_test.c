/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_asl_availability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	FILE *input;
	FILE *output;
	long length;
	char *source;
	char *artifact = NULL;
	long artifact_length;

	if (argc != 2)
		return 2;
	input = fopen(argv[1], "rb");
	if (!input || fseek(input, 0, SEEK_END) || (length = ftell(input)) < 0 ||
	    fseek(input, 0, SEEK_SET))
		return 1;
	source = malloc((size_t)length + 1U);
	if (!source || fread(source, 1, (size_t)length, input) != (size_t)length ||
	    fclose(input)) {
		free(source);
		return 1;
	}
	output = tmpfile();
	if (!output || orlix_tcti_target_asl_availability_emit(source, (size_t)length,
						    output) != ORLIX_TCTI_TARGET_ASL_AVAILABILITY_OK ||
	    fflush(output) || fseek(output, 0, SEEK_SET)) {
		free(source);
		if (output)
			fclose(output);
		return 1;
	}
	if (fseek(output, 0, SEEK_END) || (artifact_length = ftell(output)) < 0 ||
	    fseek(output, 0, SEEK_SET)) {
		free(source);
		fclose(output);
		return 1;
	}
	artifact = malloc((size_t)artifact_length + 1U);
	if (!artifact || fread(artifact, 1, (size_t)artifact_length, output) !=
		(size_t)artifact_length) {
		free(artifact);
		free(source);
		fclose(output);
		return 1;
	}
	artifact[artifact_length] = '\0';
	if (!strstr(artifact, "inline_aarchmrs_operations_v3") ||
	    !strstr(artifact, "ORLIX_TCTI_A64_ASL_CORPUS_ABSENT") ||
	    !strstr(artifact, "ORLIX_TCTI_A64_ASL_HELPERS_UNAVAILABLE") ||
	    !strstr(artifact,
		"\"operations/ABS/operation\",") ||
	    !strstr(artifact,
		"\"28fb16d9885379aa6e05267c659d8b7dab31e819051d85e1dbde8347bc2fdce8\", "
		"ORLIX_TCTI_A64_ASL_BODY_PLACEHOLDER") ||
	    !strstr(artifact,
		"\"operations/ABS/decode\",") ||
	    !strstr(artifact,
		"\"74234e98afe7498fb5daf1f36ac2d78acc339464f950703b8c019892f982b90b\", "
		"ORLIX_TCTI_A64_ASL_DECODE_NULL") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_BODY_PRESENT") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_CORPUS_PRESENT") ||
	    strstr(artifact, "ORLIX_TCTI_A64_ASL_HELPERS_AVAILABLE")) {
		free(artifact);
		free(source);
		fclose(output);
		return 1;
	}
	free(artifact);
	free(source);
	fclose(output);
	puts("PASS inline AARCHMRS operation body and decode provenance remain shared-ASL blocking");
	return 0;
}
