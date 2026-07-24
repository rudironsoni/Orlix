/* SPDX-License-Identifier: GPL-2.0-only */
#define TARGET_REGISTER_ARTIFACT_GENERATOR_NO_MAIN
#include "target_register_artifact_generator.c"

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
	char *source;
	long file_length;

	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) ||
	    (file_length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	source = malloc((size_t)file_length + 1U);
	if (!source ||
	    fread(source, 1, (size_t)file_length, file) !=
	    (size_t)file_length) {
		free(source);
		fclose(file);
		return NULL;
	}
	fclose(file);
	source[file_length] = '\0';
	*length = (size_t)file_length;
	return source;
}

static long stream_length(FILE *stream)
{
	long length;

	if (fflush(stream) || fseek(stream, 0, SEEK_END))
		return -1;
	length = ftell(stream);
	return length;
}

static int streams_equal(FILE *left, FILE *right)
{
	unsigned char left_bytes[4096];
	unsigned char right_bytes[4096];
	size_t left_count;
	size_t right_count;

	CHECK(!fseek(left, 0, SEEK_SET));
	CHECK(!fseek(right, 0, SEEK_SET));
	for (;;) {
		left_count = fread(left_bytes, 1, sizeof(left_bytes), left);
		right_count = fread(right_bytes, 1, sizeof(right_bytes), right);
		CHECK(left_count == right_count);
		CHECK(!memcmp(left_bytes, right_bytes, left_count));
		if (!left_count)
			break;
	}
	CHECK(!ferror(left));
	CHECK(!ferror(right));
	return 0;
}

static int stream_contains(FILE *stream, const char *needle)
{
	long length;
	char *bytes;
	int found;

	length = stream_length(stream);
	CHECK(length >= 0);
	CHECK(!fseek(stream, 0, SEEK_SET));
	bytes = malloc((size_t)length + 1U);
	CHECK(bytes != NULL);
	CHECK(fread(bytes, 1, (size_t)length, stream) == (size_t)length);
	bytes[length] = '\0';
	found = strstr(bytes, needle) != NULL;
	free(bytes);
	return found ? 0 : -1;
}

static int validation_failure_emits_nothing(void)
{
	struct tcti_register_model model = { 0 };
	FILE *output = tmpfile();

	CHECK(output != NULL);
	CHECK(tcti_target_register_artifact_emit_model(
		      &model, 1U, output) ==
	      TCTI_REGISTER_ARTIFACT_UNRESOLVED_MODEL);
	CHECK(stream_length(output) == 0);
	fclose(output);

	output = tmpfile();
	CHECK(output != NULL);
	CHECK(tcti_target_register_artifact_emit(
		      "[]", TCTI_REGISTER_ARTIFACT_MAX_INPUT + 1U, output) ==
	      TCTI_REGISTER_ARTIFACT_LIMIT);
	CHECK(stream_length(output) == 0);
	fclose(output);
	return 0;
}

static int pinned_determinism_and_disposition(const char *path)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error model_error = { 0 };
	char *source;
	size_t source_length;
	FILE *first = NULL;
	FILE *second = NULL;
	char *saved_type;
	size_t saved_source_offset;
	int result = -1;
	static const char *const required_macros[] = {
		"TREG_SRC(",
		"TREG_COUNTS(",
		"TREG_META(",
		"TREG_REG(",
		"TREG_MEM(",
		"TREG_IMPL(",
		"TREG_FS(",
		"TREG_F(",
		"TREG_RANGE(",
		"TREG_FB(",
		"TREG_VS(",
		"TREG_DOM(",
		"TREG_CON(",
		"TREG_CI(",
		"TREG_LINK(",
		"TREG_ACC(",
		"TREG_AO(",
		"TREG_ENC(",
		"TREG_SEL(",
		"TREG_LIT(",
		"TREG_EQ(",
		"TREG_SLICE(",
		"TREG_GRP(",
		"TREG_GF(",
		"TREG_E(",
		"TREG_EC(",
	};
	size_t index;

	source = read_file(path, &source_length);
	CHECK(source != NULL);
	if (tcti_register_model_import(source, source_length, &model,
				       &model_error)) {
		fprintf(stderr, "model import failed: %d at %zu: %s\n",
			model_error.code, model_error.offset, model_error.message);
		goto out;
	}
	first = tmpfile();
	second = tmpfile();
	CHECK(first != NULL);
	CHECK(second != NULL);
	CHECK(tcti_target_register_artifact_emit_model(
		      &model, source_length, first) == TCTI_REGISTER_ARTIFACT_OK);
	CHECK(tcti_target_register_artifact_emit_model(
		      &model, source_length, second) == TCTI_REGISTER_ARTIFACT_OK);
	CHECK(!streams_equal(first, second));
	for (index = 0;
	     index < sizeof(required_macros) / sizeof(required_macros[0]);
	     index++)
		CHECK(!stream_contains(first, required_macros[index]));

	/*
	 * A missing typed disposition must fail before changing output. Restore
	 * the owning model before destruction.
	 */
	saved_type = model.accessors[0].type;
	model.accessors[0].type = "Accessors.Unmodeled";
	fclose(second);
	second = tmpfile();
	CHECK(second != NULL);
	CHECK(tcti_target_register_artifact_emit_model(
		      &model, source_length, second) ==
	      TCTI_REGISTER_ARTIFACT_UNRESOLVED_MODEL);
	CHECK(stream_length(second) == 0);
	model.accessors[0].type = saved_type;

	/* Provenance overflow is also rejected without corrupting ownership. */
	saved_source_offset = model.expressions[0].source_offset;
	model.expressions[0].source_offset = (size_t)UINT32_MAX + 1U;
	fclose(second);
	second = tmpfile();
	CHECK(second != NULL);
	CHECK(tcti_target_register_artifact_emit_model(
		      &model, source_length, second) ==
	      TCTI_REGISTER_ARTIFACT_UNRESOLVED_MODEL);
	CHECK(stream_length(second) == 0);
	model.expressions[0].source_offset = saved_source_offset;
	result = 0;
out:
	if (first)
		fclose(first);
	if (second)
		fclose(second);
	tcti_register_model_destroy(&model);
	free(source);
	return result;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s /path/to/pinned/Registers.json\n",
			argv[0]);
		return 2;
	}
	CHECK(!validation_failure_emits_nothing());
	CHECK(!pinned_determinism_and_disposition(argv[1]));
	puts("target register artifact generator tests: passed");
	return 0;
}
