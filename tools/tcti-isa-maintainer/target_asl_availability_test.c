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
	char line[512];
	size_t rows = 0;
	size_t absent_notes = 0;

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
	if (!output || tcti_target_asl_availability_emit(source, (size_t)length,
						    output) != TCTI_TARGET_ASL_AVAILABILITY_OK ||
	    fflush(output) || fseek(output, 0, SEEK_SET)) {
		free(source);
		if (output)
			fclose(output);
		return 1;
	}
	if (!fgets(line, sizeof(line), output) ||
	    !strstr(line, "inline_aarchmrs_operations_v2") ||
	    !strstr(line, "shared_asl_absent_blocking")) {
		free(source);
		fclose(output);
		return 1;
	}
	while (fgets(line, sizeof(line), output))
		if (strstr(line, "TCTI_A64_ASL_AVAILABILITY_ROW(")) {
			rows++;
			if (strstr(line, "\"absent\", 0U, 0U, \"\""))
				absent_notes++;
		}
	free(source);
	fclose(output);
	if (rows != 4350U || absent_notes != 4350U)
		return 1;
	puts("PASS inline AARCHMRS operation and operational-note provenance remain shared-ASL blocking");
	return 0;
}
