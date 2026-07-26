// SPDX-License-Identifier: GPL-2.0-only
#include "target_register_model.h"
#include "target_system_accessor_reconciliation.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ORLIX_TCTI_SYSTEM_ACCESSOR_GENERATOR_MAX_INPUT (128U * 1024U * 1024U)

static char *read_source(const char *path, size_t *length)
{
	FILE *file;
	char *source;
	long file_length;

	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) ||
	    (file_length = ftell(file)) < 0 ||
	    (uintmax_t)file_length > ORLIX_TCTI_SYSTEM_ACCESSOR_GENERATOR_MAX_INPUT ||
	    (uintmax_t)file_length > SIZE_MAX - 1U ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	source = malloc((size_t)file_length + 1U);
	if (!source ||
	    fread(source, 1, (size_t)file_length, file) !=
		    (size_t)file_length ||
	    fclose(file)) {
		free(source);
		return NULL;
	}
	source[file_length] = '\0';
	*length = (size_t)file_length;
	return source;
}

int main(int argc, char **argv)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error import_error = { 0 };
	enum orlix_tcti_system_accessor_reconciliation_error error;
	char *source;
	size_t source_length;
	FILE *output = stdout;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s Registers.json [artifact.def]\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 3) {
		output = fopen(argv[2], "wb");
		if (!output) {
			fprintf(stderr, "%s: %s\n", argv[2], strerror(errno));
			return EXIT_FAILURE;
		}
	}
	source = read_source(argv[1], &source_length);
	if (!source) {
		fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		if (output != stdout) {
			fclose(output);
			unlink(argv[2]);
		}
		return EXIT_FAILURE;
	}
	if (orlix_tcti_register_model_import(source, source_length, &model,
				       &import_error)) {
		fprintf(stderr, "Registers.json:%zu: %s\n", import_error.offset,
			import_error.message[0] ? import_error.message :
			"register-model import failed");
		free(source);
		if (output != stdout) {
			fclose(output);
			unlink(argv[2]);
		}
		return EXIT_FAILURE;
	}
	error = orlix_tcti_system_accessor_reconciliation_emit(&model, output);
	if (output != stdout) {
		if (fflush(output) || fclose(output))
			error = ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID;
		output = NULL;
	}
	orlix_tcti_register_model_destroy(&model);
	free(source);
	if (error != ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK) {
		fprintf(stderr, "system accessor reconciliation: %s\n",
			orlix_tcti_system_accessor_reconciliation_error_name(error));
		if (output && output != stdout)
			fclose(output);
		if (argc == 3)
			unlink(argv[2]);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
