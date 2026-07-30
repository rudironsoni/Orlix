/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include "target_runtime_feature_condition_artifact_generator.c"

#include <stdio.h>
#include <stdlib.h>

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int read_file(const char *path, char **data, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;

	if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return -1;
	}
	*data = malloc((size_t)size + 1U);
	if (!*data || fread(*data, 1, (size_t)size, file) != (size_t)size ||
	    fclose(file)) {
		free(*data);
		return -1;
	}
	(*data)[size] = '\0';
	*length = (size_t)size;
	return 0;
}

static char *capture(const char *instructions, size_t instructions_length,
	const char *features, size_t features_length)
{
	FILE *file = tmpfile();
	long length;
	char *text;

	if (!file || orlix_tcti_runtime_feature_condition_artifact_emit(instructions,
		instructions_length, features, features_length, file))
		goto fail;
	if (fflush(file) || fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET))
		goto fail;
	text = malloc((size_t)length + 1U);
	if (!text || fread(text, 1, (size_t)length, file) != (size_t)length) {
		free(text);
		goto fail;
	}
	text[length] = '\0';
	fclose(file);
	return text;
fail:
	if (file)
		fclose(file);
	return NULL;
}

int main(int argc, char **argv)
{
	char *instructions = NULL, *features = NULL, *first = NULL, *second = NULL;
	size_t instructions_length, features_length;
	int status = 1;

	if (argc != 3 || read_file(argv[1], &instructions, &instructions_length) ||
	    read_file(argv[2], &features, &features_length))
		goto out;
	first = capture(instructions, instructions_length, features, features_length);
	second = capture(instructions, instructions_length, features, features_length);
	EXPECT(first && second && !strcmp(first, second));
	EXPECT(strstr(first,
		"ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_SOURCE(\"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\", \"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187\", 4350U, 409U)") != NULL);
	EXPECT(strstr(first,
		"ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE_COUNT(4350U)") != NULL);
	EXPECT(strstr(first,
		"ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE(4349U)") != NULL);
	EXPECT(orlix_tcti_runtime_feature_condition_artifact_emit("{}", 2U,
		features, features_length, tmpfile()) != 0);
	status = 0;
out:
	free(second);
	free(first);
	free(features);
	free(instructions);
	return status;
}
