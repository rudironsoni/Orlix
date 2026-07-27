/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Convert the pinned Arm Features.json into a C-native source artifact.
 *
 * This is deliberately a structural serialization.  The output must retain
 * every parsed grammar node and every ordered relation so later exact-domain
 * evaluation cannot mistake a runtime capability projection for the target.
 */
#include "target_feature_artifact_generator.h"
#include "target_feature_model.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static enum orlix_tcti_feature_artifact_error artifact_error_from_model(
	const struct orlix_tcti_feature_error *error)
{
	switch (error->code) {
	case ORLIX_TCTI_FEATURE_OK:
		return ORLIX_TCTI_FEATURE_ARTIFACT_OK;
	case ORLIX_TCTI_FEATURE_INVALID_ARGUMENT:
		return ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT;
	case ORLIX_TCTI_FEATURE_INVALID_JSON:
	case ORLIX_TCTI_FEATURE_INVALID_SOURCE:
	case ORLIX_TCTI_FEATURE_DUPLICATE_KEY:
		return ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_SOURCE;
	case ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR:
		return ORLIX_TCTI_FEATURE_ARTIFACT_UNSUPPORTED_GRAMMAR;
	case ORLIX_TCTI_FEATURE_LIMIT:
		return ORLIX_TCTI_FEATURE_ARTIFACT_LIMIT;
	case ORLIX_TCTI_FEATURE_NO_MEMORY:
		return ORLIX_TCTI_FEATURE_ARTIFACT_NO_MEMORY;
	case ORLIX_TCTI_FEATURE_PIN_MISMATCH:
		return ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH;
	case ORLIX_TCTI_FEATURE_COUNT_MISMATCH:
		return ORLIX_TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH;
	}
	return ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_SOURCE;
}

const char *orlix_tcti_target_feature_artifact_error_name(
	enum orlix_tcti_feature_artifact_error error)
{
	switch (error) {
	case ORLIX_TCTI_FEATURE_ARTIFACT_OK:
		return "success";
	case ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT:
		return "invalid argument";
	case ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_SOURCE:
		return "invalid source";
	case ORLIX_TCTI_FEATURE_ARTIFACT_UNSUPPORTED_GRAMMAR:
		return "unsupported grammar";
	case ORLIX_TCTI_FEATURE_ARTIFACT_LIMIT:
		return "resource limit";
	case ORLIX_TCTI_FEATURE_ARTIFACT_NO_MEMORY:
		return "out of memory";
	case ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH:
		return "pin mismatch";
	case ORLIX_TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH:
		return "count mismatch";
	case ORLIX_TCTI_FEATURE_ARTIFACT_IO:
		return "I/O failure";
	case ORLIX_TCTI_FEATURE_ARTIFACT_MISMATCH:
		return "artifact mismatch";
	}
	return "unknown failure";
}

/* Octal escapes are fixed width, unlike \x escapes followed by hex text. */
static int emit_c_string(FILE *output, const char *text)
{
	const unsigned char *cursor = (const unsigned char *)text;

	if (fputc('"', output) == EOF)
		return -1;
	while (*cursor) {
		if (*cursor == '"' || *cursor == '\\') {
			if (fputc('\\', output) == EOF || fputc(*cursor, output) == EOF)
				return -1;
		} else if (*cursor >= 0x20U && *cursor <= 0x7eU) {
			if (fputc(*cursor, output) == EOF)
				return -1;
		} else if (fprintf(output, "\\%03o", (unsigned int)*cursor) < 0) {
			return -1;
		}
		cursor++;
	}
	return fputc('"', output) == EOF ? -1 : 0;
}

static int emit_provenance(FILE *output,
	const struct orlix_tcti_feature_provenance *provenance)
{
	return fprintf(output, "%zuU, %zuU", provenance->offset,
		       provenance->length) < 0 ? -1 : 0;
}

static int emit_field_qualifier(FILE *output,
	const struct orlix_tcti_feature_field_qualifier *qualifier)
{
	return fprintf(output, "%uU, ", (unsigned int)qualifier->kind) < 0 ||
		emit_provenance(output, &qualifier->provenance);
}

static int emit_node(FILE *output, size_t index,
	const struct orlix_tcti_feature_node *node)
{
	if (fprintf(output, "ORLIX_TCTI_A64_FEATURE_NODE(%zuU, %uU, %" PRIu32
		    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
		    "U, %" PRId64 ", ", index, (unsigned int)node->kind,
		    node->left, node->right, node->first_child, node->child_count,
		    node->integer) < 0 ||
	    emit_c_string(output, node->text ? node->text : "") ||
	    fputs(", ", output) == EOF ||
	    emit_c_string(output, node->field.state ? node->field.state : "") ||
	    fputs(", ", output) == EOF ||
	    emit_c_string(output, node->field.register_name ?
			  node->field.register_name : "") ||
	    fputs(", ", output) == EOF ||
	    emit_c_string(output, node->field.selector ? node->field.selector : "") ||
	    fputs(", ", output) == EOF ||
	    emit_provenance(output, &node->field.provenance) ||
	    fputs(", ", output) == EOF ||
	    emit_field_qualifier(output, &node->field.instance) ||
	    fputs(", ", output) == EOF ||
	    emit_field_qualifier(output, &node->field.slices) ||
	    fputs(", ", output) == EOF ||
	    emit_provenance(output, &node->provenance) ||
	    fputs(")\n", output) == EOF)
		return -1;
	return 0;
}

static int emit_artifact(FILE *output, const struct orlix_tcti_feature_model *model,
	size_t source_length)
{
	size_t index;
	size_t parameter_constraint_count = 0;

	for (index = 0; index < model->parameter_count; index++)
		parameter_constraint_count += model->parameters[index].constraint_count;
	if (parameter_constraint_count > model->constraint_count)
		return -1;
	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated by target_feature_artifact_generator.c. Do not edit. */\n"
		  "ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(\"vFATAp1-A\", \"818\", "
		  "\"2026-06_rel\", \"2.9.5\", "
		  "\"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187\", ",
		  output) == EOF ||
	    fprintf(output, "%zuU)\n", source_length) < 0 ||
	    fprintf(output, "ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(%zuU, %zuU, %zuU, "
		    "%zuU, %zuU, %zuU)\n", model->parameter_count,
		    model->constraint_count, parameter_constraint_count,
		    model->node_count, model->child_count,
		    model->constraint_count - parameter_constraint_count) < 0)
		return -1;
	for (index = 0; index < model->parameter_count; index++) {
		const struct orlix_tcti_feature_parameter *parameter =
			&model->parameters[index];

		if (fprintf(output, "ORLIX_TCTI_A64_FEATURE_PARAMETER(%zuU, ", index) < 0 ||
		    emit_c_string(output, parameter->name) ||
		    fprintf(output, ", %" PRIu32 "U, %" PRIu32 "U, ",
			    parameter->first_constraint, parameter->constraint_count) < 0 ||
		    emit_provenance(output, &parameter->provenance) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->constraint_count; index++) {
		const struct orlix_tcti_feature_node *constraint =
			&model->nodes[model->constraints[index]];

		if (fprintf(output, "ORLIX_TCTI_A64_FEATURE_CONSTRAINT(%zuU, %" PRIu32
			    "U, ", index, model->constraints[index]) < 0 ||
		    emit_provenance(output, &constraint->provenance) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->node_count; index++)
		if (emit_node(output, index, &model->nodes[index]))
			return -1;
	for (index = 0; index < model->child_count; index++)
		if (fprintf(output, "ORLIX_TCTI_A64_FEATURE_CHILD(%zuU, %" PRIu32
			    "U)\n", index, model->children[index]) < 0)
			return -1;
	return ferror(output) ? -1 : 0;
}

enum orlix_tcti_feature_artifact_error orlix_tcti_target_feature_artifact_emit(
	const char *source, size_t length, FILE *output)
{
	struct orlix_tcti_feature_model model = { 0 };
	struct orlix_tcti_feature_error error = { 0 };
	enum orlix_tcti_feature_artifact_error result;

	if (!source || !length || !output)
		return ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT;
	/* Bound hostile input before the importer can allocate parser state. */
	if (length > ORLIX_TCTI_FEATURE_ARTIFACT_MAX_INPUT)
		return ORLIX_TCTI_FEATURE_ARTIFACT_LIMIT;
	/* Import completes before emit_artifact can write a single byte. */
	if (orlix_tcti_target_feature_model_import(source, length, &model, &error))
		return artifact_error_from_model(&error);
	if (length != ORLIX_TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH) {
		orlix_tcti_target_feature_model_destroy(&model);
		return ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH;
	}
	result = emit_artifact(output, &model,
		ORLIX_TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH) ?
		ORLIX_TCTI_FEATURE_ARTIFACT_IO :
		ORLIX_TCTI_FEATURE_ARTIFACT_OK;
	orlix_tcti_target_feature_model_destroy(&model);
	return result;
}

#ifndef TARGET_FEATURE_ARTIFACT_GENERATOR_NO_MAIN
struct source_bytes {
	char *data;
	size_t length;
};

static enum orlix_tcti_feature_artifact_error read_source(
	const char *path, struct source_bytes *source)
{
	FILE *file;
	long length;
	size_t read_count;
	int close_result;

	file = fopen(path, "rb");
	if (!file)
		return ORLIX_TCTI_FEATURE_ARTIFACT_IO;
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    (uintmax_t)length > SIZE_MAX - 1) {
		fclose(file);
		return ORLIX_TCTI_FEATURE_ARTIFACT_IO;
	}
	/* Reject the file before rewind, allocation, or reading any source byte. */
	if ((uintmax_t)length > ORLIX_TCTI_FEATURE_ARTIFACT_MAX_INPUT) {
		fclose(file);
		return ORLIX_TCTI_FEATURE_ARTIFACT_LIMIT;
	}
	if (fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return ORLIX_TCTI_FEATURE_ARTIFACT_IO;
	}
	source->data = malloc((size_t)length + 1);
	if (!source->data) {
		fclose(file);
		return ORLIX_TCTI_FEATURE_ARTIFACT_NO_MEMORY;
	}
	read_count = fread(source->data, 1, (size_t)length, file);
	close_result = fclose(file);
	if (read_count != (size_t)length || close_result) {
		free(source->data);
		*source = (struct source_bytes){ 0 };
		return ORLIX_TCTI_FEATURE_ARTIFACT_IO;
	}
	source->data[length] = '\0';
	source->length = (size_t)length;
	return ORLIX_TCTI_FEATURE_ARTIFACT_OK;
}

static int files_match(FILE *left, FILE *right)
{
	unsigned char left_bytes[4096];
	unsigned char right_bytes[4096];
	size_t left_count;
	size_t right_count;

	for (;;) {
		left_count = fread(left_bytes, 1, sizeof(left_bytes), left);
		right_count = fread(right_bytes, 1, sizeof(right_bytes), right);
		if (left_count != right_count ||
		    memcmp(left_bytes, right_bytes, left_count))
			return -1;
		if (!left_count)
			return ferror(left) || ferror(right) ? -1 : 0;
	}
}

int main(int argc, char **argv)
{
	struct source_bytes source = { 0 };
	FILE *output = stdout;
	FILE *expected = NULL;
	enum orlix_tcti_feature_artifact_error error;
	enum orlix_tcti_feature_artifact_error read_error;
	int status = EXIT_FAILURE;

	if (argc == 2) {
		/* output remains stdout */
	} else if (argc == 4 && !strcmp(argv[1], "--check")) {
		expected = fopen(argv[3], "rb");
		output = tmpfile();
		if (!expected || !output) {
			fprintf(stderr, "target feature artifact generator: I/O failure\n");
			goto out;
		}
	} else {
		fprintf(stderr, "usage: %s Features.json\n"
			"       %s --check Features.json artifact.def\n",
			argv[0], argv[0]);
		goto out;
	}
	read_error = read_source(argc == 2 ? argv[1] : argv[2], &source);
	if (read_error != ORLIX_TCTI_FEATURE_ARTIFACT_OK) {
		fprintf(stderr, "target feature artifact generator: %s\n",
			orlix_tcti_target_feature_artifact_error_name(read_error));
		goto out;
	}
	error = orlix_tcti_target_feature_artifact_emit(source.data, source.length,
			output);
	if (error != ORLIX_TCTI_FEATURE_ARTIFACT_OK) {
		fprintf(stderr, "target feature artifact generator: %s\n",
			orlix_tcti_target_feature_artifact_error_name(error));
		goto out;
	}
	if (expected && (fflush(output) || fseek(output, 0, SEEK_SET) ||
			 files_match(expected, output))) {
		fprintf(stderr, "target feature artifact generator: artifact mismatch\n");
		goto out;
	}
	status = EXIT_SUCCESS;
out:
	if (expected)
		fclose(expected);
	if (output != stdout)
		fclose(output);
	free(source.data);
	return status;
}
#endif
