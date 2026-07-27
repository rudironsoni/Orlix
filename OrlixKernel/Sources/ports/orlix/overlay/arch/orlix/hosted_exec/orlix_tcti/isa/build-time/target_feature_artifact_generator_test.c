/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#define TARGET_FEATURE_ARTIFACT_GENERATOR_NO_MAIN
#include "target_feature_artifact_generator.c"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static char *read_file(const char *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;
	char *source;

	if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    (uintmax_t)size > ORLIX_TCTI_FEATURE_ARTIFACT_MAX_INPUT ||
	    fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	source = malloc((size_t)size + 1);
	if (!source || fread(source, 1, (size_t)size, file) != (size_t)size) {
		free(source);
		fclose(file);
		return NULL;
	}
	fclose(file);
	source[size] = '\0';
	*length = (size_t)size;
	return source;
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
	copy = malloc(source_length - from_length + to_length + 1);
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

static char *read_stream(FILE *stream, size_t *length)
{
	long size;
	char *result;

	if (fflush(stream) || fseek(stream, 0, SEEK_END) ||
	    (size = ftell(stream)) < 0 || fseek(stream, 0, SEEK_SET))
		return NULL;
	result = malloc((size_t)size + 1);
	if (!result || fread(result, 1, (size_t)size, stream) != (size_t)size) {
		free(result);
		return NULL;
	}
	result[size] = '\0';
	*length = (size_t)size;
	return result;
}

static int require_empty_failure(const char *source, size_t length,
	enum orlix_tcti_feature_artifact_error expected)
{
	FILE *output = tmpfile();

	CHECK(output != NULL);
	CHECK(orlix_tcti_target_feature_artifact_emit(source, length, output) == expected);
	CHECK(fflush(output) == 0);
	CHECK(ftell(output) == 0);
	fclose(output);
	return 0;
}

static int pinned_source_is_structural_and_deterministic(const char *source,
	size_t source_length)
{
	FILE *first = tmpfile();
	FILE *second = tmpfile();
	struct orlix_tcti_feature_model model = { 0 };
	struct orlix_tcti_feature_error error = { 0 };
	char *first_text;
	char *second_text;
	size_t first_length;
	size_t second_length;
	size_t index;
	size_t field_count = 0;

	CHECK(first && second);
	CHECK(orlix_tcti_target_feature_artifact_emit(source, source_length, first) ==
	      ORLIX_TCTI_FEATURE_ARTIFACT_OK);
	CHECK(orlix_tcti_target_feature_artifact_emit(source, source_length, second) ==
	      ORLIX_TCTI_FEATURE_ARTIFACT_OK);
	first_text = read_stream(first, &first_length);
	second_text = read_stream(second, &second_length);
	CHECK(first_text && second_text);
	CHECK(first_length == second_length);
	CHECK(!memcmp(first_text, second_text, first_length));
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(409U, 1626U,") != NULL);
	CHECK(strstr(first_text,
		"ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(\"vFATAp1-A\", \"818\", "
		"\"2026-06_rel\", \"2.9.5\", "
		"\"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187\", "
		"1243621U)") != NULL);
	CHECK(source_length == ORLIX_TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_PARAMETER(0U,") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_CONSTRAINT(1625U,") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_NODE(") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_CHILD(") != NULL);
	CHECK(strstr(first_text, "ORLIX_TCTI_A64_FEATURE_NODE(0U,") != NULL);
	CHECK(orlix_tcti_target_feature_model_import(source, source_length, &model,
		&error) == 0);
	for (index = 0; index < model.parameter_count; index++) {
		char marker[64];

		CHECK(snprintf(marker, sizeof(marker),
			"ORLIX_TCTI_A64_FEATURE_PARAMETER(%zuU,", index) > 0);
		CHECK(strstr(first_text, marker) != NULL);
	}
	for (index = 0; index < model.constraint_count; index++) {
		char marker[64];

		CHECK(snprintf(marker, sizeof(marker),
			"ORLIX_TCTI_A64_FEATURE_CONSTRAINT(%zuU, %" PRIu32 "U,",
			index, model.constraints[index]) > 0);
		CHECK(strstr(first_text, marker) != NULL);
	}
	for (index = 0; index < model.node_count; index++) {
		char marker[80];

		CHECK(snprintf(marker, sizeof(marker),
			"ORLIX_TCTI_A64_FEATURE_NODE(%zuU, %uU,", index,
			(unsigned int)model.nodes[index].kind) > 0);
		CHECK(strstr(first_text, marker) != NULL);
	}
	for (index = 0; index < model.node_count; index++) {
		const struct orlix_tcti_feature_node *node = &model.nodes[index];
		char marker[512];

		if (node->kind != ORLIX_TCTI_FEATURE_FIELD)
			continue;
		field_count++;
		CHECK(node->field.instance.kind ==
		      ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NULL);
		CHECK(node->field.slices.kind == ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NULL);
		CHECK(snprintf(marker, sizeof(marker),
			"ORLIX_TCTI_A64_FEATURE_NODE(%zuU, %uU,", index,
			(unsigned int)node->kind) > 0);
		CHECK(strstr(first_text, marker) != NULL);
		CHECK(node->field.instance.provenance.length == 4U);
		CHECK(node->field.slices.provenance.length == 4U);
		CHECK(snprintf(marker, sizeof(marker),
			"\"%s\", \"%s\", %zuU, %zuU, %uU, %zuU, %zuU, "
			"%uU, %zuU, %zuU,",
			node->field.register_name, node->field.selector,
			node->field.provenance.offset, node->field.provenance.length,
			(unsigned int)node->field.instance.kind,
			node->field.instance.provenance.offset,
			node->field.instance.provenance.length,
			(unsigned int)node->field.slices.kind,
			node->field.slices.provenance.offset,
			node->field.slices.provenance.length) > 0);
		CHECK(strstr(first_text, marker) != NULL);
	}
	CHECK(field_count == 605U);
	for (index = 0; index < model.child_count; index++) {
		char marker[80];

		CHECK(snprintf(marker, sizeof(marker),
			"ORLIX_TCTI_A64_FEATURE_CHILD(%zuU, %" PRIu32 "U)", index,
			model.children[index]) > 0);
		CHECK(strstr(first_text, marker) != NULL);
	}
	orlix_tcti_target_feature_model_destroy(&model);
	free(first_text);
	free(second_text);
	fclose(first);
	fclose(second);
	return 0;
}

static int malformed_pin_unknown_and_limit_emit_nothing(const char *source,
	size_t source_length)
{
	static const char embedded_nul_source[] =
		"{\"_type\":\"Features\",\"_meta\":{\"version\":{"
		"\"architecture\":\"vFATAp1-A\",\"build\":\"818\","
		"\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\"}},"
		"\"parameters\":[{\"_type\":\"Parameters.Boolean\","
		"\"name\":\"x\\u0000y\",\"constraints\":[]}],"
		"\"constraints\":[]}";
	char *mutation;
	size_t mutation_length;

	CHECK(require_empty_failure("{\"parameters\":[", strlen("{\"parameters\":["),
		ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_SOURCE) == 0);
	CHECK(require_empty_failure(embedded_nul_source,
		strlen(embedded_nul_source),
		ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_SOURCE) == 0);
	mutation = replace_once(source, source_length, "\"AST.Bool\"", "\"AST.Nope\"",
		&mutation_length);
	CHECK(mutation != NULL);
	CHECK(require_empty_failure(mutation, mutation_length,
		ORLIX_TCTI_FEATURE_ARTIFACT_UNSUPPORTED_GRAMMAR) == 0);
	free(mutation);
	mutation = replace_once(source, source_length, "\"Features\"", "\"Features\" ",
		&mutation_length);
	CHECK(mutation != NULL);
	CHECK(require_empty_failure(mutation, mutation_length,
		ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH) == 0);
	free(mutation);
	CHECK(require_empty_failure(source, ORLIX_TCTI_FEATURE_ARTIFACT_MAX_INPUT + 1U,
		ORLIX_TCTI_FEATURE_ARTIFACT_LIMIT) == 0);
	return 0;
}

static int oversized_cli_is_rejected_before_read(const char *generator)
{
	char path[] = "/tmp/orlix-feature-artifact-oversized-XXXXXX";
	FILE *output = NULL;
	FILE *diagnostics = NULL;
	char *diagnostic_text = NULL;
	size_t diagnostic_length;
	int descriptor;
	int status;
	pid_t child;
	int result = -1;

	descriptor = mkstemp(path);
	if (descriptor < 0)
		return -1;
	if (ftruncate(descriptor,
		      (off_t)ORLIX_TCTI_FEATURE_ARTIFACT_MAX_INPUT + 1) ||
	    close(descriptor))
		goto out;
	descriptor = -1;
	output = tmpfile();
	diagnostics = tmpfile();
	if (!output || !diagnostics)
		goto out;
	child = fork();
	if (child < 0)
		goto out;
	if (!child) {
		if (dup2(fileno(output), STDOUT_FILENO) < 0 ||
		    dup2(fileno(diagnostics), STDERR_FILENO) < 0)
			_exit(127);
		execl(generator, generator, path, (char *)NULL);
		_exit(127);
	}
	if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
	    WEXITSTATUS(status) == 0)
		goto out;
	if (fflush(output) || fseek(output, 0, SEEK_END) || ftell(output) != 0)
		goto out;
	diagnostic_text = read_stream(diagnostics, &diagnostic_length);
	if (!diagnostic_text || !diagnostic_length ||
	    !strstr(diagnostic_text,
		    "target feature artifact generator: resource limit"))
		goto out;
	result = 0;
out:
	if (descriptor >= 0)
		close(descriptor);
	if (output)
		fclose(output);
	if (diagnostics)
		fclose(diagnostics);
	free(diagnostic_text);
	unlink(path);
	return result;
}

int main(int argc, char **argv)
{
	const char *generator = NULL;
	const char *source_path;
	char *source;
	size_t source_length;

	if (argc == 2) {
		source_path = argv[1];
	} else if (argc == 3) {
		generator = argv[1];
		source_path = argv[2];
	} else {
		fprintf(stderr, "usage: %s [generator] Features.json\n", argv[0]);
		return 2;
	}
	source = read_file(source_path, &source_length);
	if (!source) {
		perror(source_path);
		return 1;
	}
	if (pinned_source_is_structural_and_deterministic(source, source_length) ||
	    malformed_pin_unknown_and_limit_emit_nothing(source, source_length) ||
	    (generator && oversized_cli_is_rejected_before_read(generator))) {
		free(source);
		return 1;
	}
	free(source);
	puts("PASS target feature artifact generator structural and no-output checks");
	return 0;
}
