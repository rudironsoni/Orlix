/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include "target_asl_availability.h"
#include "target_feature_artifact_generator.h"
#include "target_feature_field_domain_binding_artifact_generator.h"
#include "target_runtime_capability_cohort_artifact_generator.h"
#include "target_instruction_artifact_generator.h"
#include "target_manifest_generator.h"
#include "target_refresh.h"
#include "target_register_artifact_generator.h"
#include "target_system_accessor_reconciliation.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define ORLIX_TCTI_TARGET_REFRESH_MAX_SOURCE (128U * 1024U * 1024U)
#define ORLIX_TCTI_TARGET_REFRESH_PUBLISH_NAME "generations"
#define ORLIX_TCTI_TARGET_REFRESH_GENERATION_PREFIX "aarchmrs-2026-06-v2"
#define ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"
#define ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"
#define ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256 \
	"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"
#define ORLIX_TCTI_TARGET_REFRESH_SCHEMA "orlix-tcti-aarchmrs-source-v2"
#define ORLIX_TCTI_TARGET_REFRESH_GENERATOR "orlix-tcti-isa-maintainer"

struct source_bytes {
	char *data;
	size_t length;
};

struct artifact_bytes {
	char *data;
	size_t length;
};

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))

static size_t count_token(const struct artifact_bytes *artifact,
			  const char *token)
{
	const char *cursor = artifact->data;
	size_t count = 0;
	size_t token_length = strlen(token);

	while (cursor && (cursor = strstr(cursor, token))) {
		count++;
		cursor += token_length;
	}
	return count;
}

static int has_token(const struct artifact_bytes *artifact, const char *token)
{
	return count_token(artifact, token) != 0;
}

static int parse_counts(const struct artifact_bytes *artifact,
			const char *marker, size_t *values, size_t count)
{
	const char *cursor = strstr(artifact->data, marker);
	size_t index;

	if (!cursor)
		return -1;
	cursor += strlen(marker);
	for (index = 0; index < count; index++) {
		char *end;
		unsigned long long value;

		while (*cursor == ' ' || *cursor == '\t')
			cursor++;
		errno = 0;
		value = strtoull(cursor, &end, 10);
		if (errno || end == cursor || value > SIZE_MAX)
			return -1;
		values[index] = (size_t)value;
		cursor = end;
		if (*cursor == 'U')
			cursor++;
		while (*cursor == ' ' || *cursor == '\t')
			cursor++;
		if (index + 1U < count) {
			if (*cursor++ != ',')
				return -1;
		} else if (*cursor != ')') {
			return -1;
		}
	}
	return 0;
}

static int validate_artifact_bundle(const struct artifact_bytes *manifest,
	const struct artifact_bytes *asl_availability,
	const struct artifact_bytes *instruction_artifact,
	const struct artifact_bytes *feature_artifact,
	const struct artifact_bytes *feature_field_domains,
	const struct artifact_bytes *runtime_capability_cohort,
	const struct artifact_bytes *register_artifact,
	const struct artifact_bytes *system_accessors)
{
	size_t feature_counts[6], field_counts[4], cohort_counts[4];
	size_t register_counts[24], accessor_counts[8];
	size_t index;
	size_t accessor_outcomes = 0;

	/* Every instruction-derived artifact binds the same pinned source. */
	if (!has_token(manifest, "ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(") ||
	    !has_token(manifest, ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    count_token(manifest, "ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(") != 4350U ||
	    !has_token(asl_availability, "ORLIX_TCTI_A64_ASL_AVAILABILITY_SOURCE(") ||
	    !has_token(asl_availability,
		       ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    count_token(asl_availability,
			"ORLIX_TCTI_A64_ASL_AVAILABILITY_ROW(") != 4350U ||
	    !has_token(instruction_artifact,
		       "ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_SOURCE_SHA256 \"") ||
	    !has_token(instruction_artifact,
		       ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    !has_token(instruction_artifact,
		       "ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT 4350U") ||
	    !has_token(instruction_artifact,
		       "orlix_tcti_a64_instruction_artifact_leaves[4350]") ||
	    !has_token(runtime_capability_cohort,
		       ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256))
		return -1;

	/* Feature identity is shared by its model, field bindings, and cohorts. */
	if (!has_token(feature_artifact,
		       "ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(") ||
	    !has_token(feature_artifact, ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256) ||
	    !has_token(feature_field_domains,
		       "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(") ||
	    !has_token(feature_field_domains,
		       ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256) ||
	    !has_token(runtime_capability_cohort,
		       "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE(") ||
	    !has_token(runtime_capability_cohort,
		       ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256))
		return -1;

	if (parse_counts(feature_artifact,
			 "ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(",
			 feature_counts, ARRAY_SIZE(feature_counts)) ||
	    count_token(feature_artifact,
			"ORLIX_TCTI_A64_FEATURE_PARAMETER(") != feature_counts[0] ||
	    count_token(feature_artifact,
			"ORLIX_TCTI_A64_FEATURE_CONSTRAINT(") != feature_counts[1] ||
	    count_token(feature_artifact,
			"ORLIX_TCTI_A64_FEATURE_CHILD(") != feature_counts[4] ||
	    feature_counts[2] + feature_counts[5] != feature_counts[1])
		return -1;

	if (parse_counts(feature_field_domains,
			 "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS(",
			 field_counts, ARRAY_SIZE(field_counts)) ||
	    !has_token(feature_field_domains,
		       ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256) ||
	    count_token(feature_field_domains,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE(") !=
		field_counts[0] ||
	    field_counts[2] + field_counts[3] != field_counts[0] ||
	    count_token(feature_field_domains,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(") != 1U)
		return -1;

	if (parse_counts(runtime_capability_cohort,
			 "ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(",
			 cohort_counts, ARRAY_SIZE(cohort_counts)) ||
	    cohort_counts[0] != 4350U || cohort_counts[1] != cohort_counts[2] ||
	    cohort_counts[1] != cohort_counts[3] ||
	    count_token(runtime_capability_cohort,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(") !=
		cohort_counts[0] ||
	    count_token(runtime_capability_cohort,
			"ORLIX_TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(") !=
		cohort_counts[1])
		return -1;

	/* Both register-derived artifacts bind one register source and reconcile
	 * every emitted system accessor into exactly one outcome bucket. */
	if (!has_token(register_artifact, "TREG_SRC(") ||
	    !has_token(register_artifact,
		       ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256) ||
	    parse_counts(register_artifact, "TREG_COUNTS(", register_counts,
			 ARRAY_SIZE(register_counts)) ||
	    count_token(register_artifact, "TREG_REG(") != register_counts[0] ||
	    !has_token(system_accessors,
		       "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(") ||
	    !has_token(system_accessors,
		       ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256) ||
	    parse_counts(system_accessors,
			 "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(",
			 accessor_counts, ARRAY_SIZE(accessor_counts)) ||
	    count_token(system_accessors,
			"ORLIX_TCTI_A64_SYSTEM_ACCESSOR(") != accessor_counts[0] ||
	    count_token(system_accessors,
			"ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(") != 1U)
		return -1;
	for (index = 1; index < ARRAY_SIZE(accessor_counts); index++)
		accessor_outcomes += accessor_counts[index];
	return accessor_outcomes == accessor_counts[0] ? 0 : -1;
}

static void set_result(struct orlix_tcti_target_refresh_result *result,
			       enum orlix_tcti_target_refresh_error error)
{
	if (result)
		result->error = error;
}

const char *orlix_tcti_target_refresh_error_name(enum orlix_tcti_target_refresh_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_REFRESH_OK: return "success";
	case ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO: return "source I/O failure";
	case ORLIX_TCTI_TARGET_REFRESH_SOURCE_LIMIT: return "source exceeds refresh limit";
	case ORLIX_TCTI_TARGET_REFRESH_SOURCE_IDENTITY: return "source SHA-256 does not match pinned Arm release";
	case ORLIX_TCTI_TARGET_REFRESH_PARSE: return "injected parse failure";
	case ORLIX_TCTI_TARGET_REFRESH_VALIDATION: return "injected cross-artifact validation failure";
	case ORLIX_TCTI_TARGET_REFRESH_MANIFEST: return "manifest generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_ASL_AVAILABILITY: return "ASL availability generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS: return "instruction artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_FEATURES: return "feature artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_FEATURE_FIELD_DOMAINS:
		return "feature field-domain binding artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_RUNTIME_CAPABILITY_COHORT:
		return "runtime capability cohort artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_REGISTERS: return "register artifact generation failed";
	case ORLIX_TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS: return "system accessor reconciliation failed";
	case ORLIX_TCTI_TARGET_REFRESH_PUBLISH: return "transactional publication failed";
	}
	return "unknown refresh failure";
}

static int read_source(const char *path, struct source_bytes *source,
		       enum orlix_tcti_target_refresh_error *error)
{
	FILE *file;
	long length;
	size_t count;

	file = fopen(path, "rb");
	if (!file) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		return -1;
	}
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > ORLIX_TCTI_TARGET_REFRESH_MAX_SOURCE) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_LIMIT;
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > SIZE_MAX - 1U || fseek(file, 0, SEEK_SET)) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	source->data = malloc((size_t)length + 1U);
	if (!source->data) {
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	count = fread(source->data, 1, (size_t)length, file);
	if (count != (size_t)length || fclose(file)) {
		free(source->data);
		*source = (struct source_bytes) { 0 };
		*error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO;
		return -1;
	}
	source->data[length] = '\0';
	source->length = (size_t)length;
	return 0;
}

static int capture(FILE *output, struct artifact_bytes *artifact)
{
	long length;

	if (fflush(output) || fseek(output, 0, SEEK_END) ||
	    (length = ftell(output)) < 0 || (uintmax_t)length > SIZE_MAX ||
	    fseek(output, 0, SEEK_SET))
		return -1;
	artifact->data = malloc((size_t)length + 1U);
	if (!artifact->data)
		return -1;
	if (fread(artifact->data, 1, (size_t)length, output) != (size_t)length) {
		free(artifact->data);
		*artifact = (struct artifact_bytes) { 0 };
		return -1;
	}
	artifact->data[length] = '\0';
	artifact->length = (size_t)length;
	return 0;
}

static int emit_manifest(const struct source_bytes *source,
			 struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = target_manifest_generator_emit(source->data, source->length,
					output) == ORLIX_TCTI_TARGET_MANIFEST_GENERATOR_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_asl_availability(const struct source_bytes *source,
				 struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_asl_availability_emit(source->data, source->length,
			output) == ORLIX_TCTI_TARGET_ASL_AVAILABILITY_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_instructions(const struct source_bytes *source,
			     struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_instruction_artifact_emit(source->data, source->length,
			output) == ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_features(const struct source_bytes *source,
			 struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_feature_artifact_emit(source->data, source->length,
			output) == ORLIX_TCTI_FEATURE_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_registers(const struct source_bytes *source,
			  struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_register_artifact_emit(source->data, source->length,
			output) == ORLIX_TCTI_REGISTER_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_feature_field_domains(const struct source_bytes *features,
				      const struct source_bytes *registers,
				      struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_target_feature_field_domain_binding_artifact_emit(
		features->data, features->length, registers->data, registers->length,
		output) == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_runtime_capability_cohort(const struct source_bytes *instructions,
	const struct source_bytes *features, struct artifact_bytes *artifact)
{
	FILE *output = tmpfile();
	int result;

	if (!output)
		return -1;
	result = orlix_tcti_runtime_capability_cohort_artifact_emit(
		instructions->data, instructions->length, features->data,
		features->length, output) ==
		ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_system_accessors(const struct source_bytes *source,
				 struct artifact_bytes *artifact)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error import_error = { 0 };
	FILE *output = tmpfile();
	int result = -1;

	if (!output)
		return -1;
	if (!orlix_tcti_register_model_import(source->data, source->length, &model,
					&import_error) &&
	    orlix_tcti_system_accessor_reconciliation_emit(&model, output) ==
		ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK && !capture(output, artifact))
		result = 0;
	orlix_tcti_register_model_destroy(&model);
	fclose(output);
	return result;
}

int orlix_tcti_target_refresh_with_fault(
	int canonical_root_fd, const char *instructions_path,
	const char *features_path, const char *registers_path,
	const struct orlix_tcti_target_refresh_fault *fault,
	struct orlix_tcti_target_refresh_result *result)
{
	struct source_bytes instructions = { 0 }, features = { 0 }, registers = { 0 };
	struct artifact_bytes manifest = { 0 }, asl_availability = { 0 };
	struct artifact_bytes instruction_artifact = { 0 };
	struct artifact_bytes feature_artifact = { 0 }, register_artifact = { 0 };
	struct artifact_bytes feature_field_domains = { 0 };
	struct artifact_bytes runtime_capability_cohort = { 0 };
	struct artifact_bytes system_accessors = { 0 };
	struct orlix_tcti_target_artifact artifacts[8];
	struct orlix_tcti_target_artifact_provenance provenance = {
		.schema = ORLIX_TCTI_TARGET_REFRESH_SCHEMA,
		.generator = ORLIX_TCTI_TARGET_REFRESH_GENERATOR,
	};
	struct orlix_tcti_target_artifact_publish_result publish_result = { 0 };
	char instruction_digest[65];
	char feature_digest[65];
	char register_digest[65];
	enum orlix_tcti_target_refresh_error error = ORLIX_TCTI_TARGET_REFRESH_OK;

	if (result)
		*result = (struct orlix_tcti_target_refresh_result) { 0 };
	if (canonical_root_fd < 0 || !instructions_path || !features_path ||
	    !registers_path) {
		set_result(result, ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT);
		errno = EINVAL;
		return -1;
	}
	if (read_source(instructions_path, &instructions, &error) ||
	    read_source(features_path, &features, &error) ||
	    read_source(registers_path, &registers, &error))
		goto out;
	orlix_tcti_target_artifact_sha256(instructions.data, instructions.length,
					 instruction_digest);
	orlix_tcti_target_artifact_sha256(features.data, features.length,
					 feature_digest);
	orlix_tcti_target_artifact_sha256(registers.data, registers.length,
					 register_digest);
	if (strcmp(instruction_digest,
		   ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS_SHA256) ||
	    strcmp(feature_digest, ORLIX_TCTI_TARGET_REFRESH_FEATURES_SHA256) ||
	    strcmp(register_digest, ORLIX_TCTI_TARGET_REFRESH_REGISTERS_SHA256)) {
		error = ORLIX_TCTI_TARGET_REFRESH_SOURCE_IDENTITY;
		errno = EINVAL;
		goto out;
	}
	provenance.instructions_sha256 = instruction_digest;
	provenance.features_sha256 = feature_digest;
	provenance.registers_sha256 = register_digest;
	if (fault && fault->stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_PARSE) {
		error = ORLIX_TCTI_TARGET_REFRESH_PARSE;
		errno = EIO;
		goto out;
	}

	if (emit_manifest(&instructions, &manifest)) {
		error = ORLIX_TCTI_TARGET_REFRESH_MANIFEST;
		goto out;
	}
	if (emit_asl_availability(&instructions, &asl_availability)) {
		error = ORLIX_TCTI_TARGET_REFRESH_ASL_AVAILABILITY;
		goto out;
	}
	if (emit_instructions(&instructions, &instruction_artifact)) {
		error = ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS;
		goto out;
	}
	if (emit_features(&features, &feature_artifact)) {
		error = ORLIX_TCTI_TARGET_REFRESH_FEATURES;
		goto out;
	}
	if (emit_feature_field_domains(&features, &registers,
				       &feature_field_domains)) {
		error = ORLIX_TCTI_TARGET_REFRESH_FEATURE_FIELD_DOMAINS;
		goto out;
	}
	if (emit_runtime_capability_cohort(&instructions, &features,
					   &runtime_capability_cohort)) {
		error = ORLIX_TCTI_TARGET_REFRESH_RUNTIME_CAPABILITY_COHORT;
		goto out;
	}
	if (emit_registers(&registers, &register_artifact)) {
		error = ORLIX_TCTI_TARGET_REFRESH_REGISTERS;
		goto out;
	}
	if (emit_system_accessors(&registers, &system_accessors)) {
		error = ORLIX_TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS;
		goto out;
	}

	artifacts[0] = (struct orlix_tcti_target_artifact) {
		.name = "source_manifest.def",
		.data = manifest.data,
		.length = manifest.length,
	};
	artifacts[1] = (struct orlix_tcti_target_artifact) {
		.name = "target_asl_availability.def",
		.data = asl_availability.data,
		.length = asl_availability.length,
	};
	artifacts[2] = (struct orlix_tcti_target_artifact) {
		.name = "target_feature_artifact.def",
		.data = feature_artifact.data,
		.length = feature_artifact.length,
	};
	artifacts[3] = (struct orlix_tcti_target_artifact) {
		.name = "target_feature_field_domain_binding.def",
		.data = feature_field_domains.data,
		.length = feature_field_domains.length,
	};
	artifacts[4] = (struct orlix_tcti_target_artifact) {
		.name = "target_instruction_artifact_generated.h",
		.data = instruction_artifact.data,
		.length = instruction_artifact.length,
	};
	artifacts[5] = (struct orlix_tcti_target_artifact) {
		.name = "target_register_artifact.def",
		.data = register_artifact.data,
		.length = register_artifact.length,
	};
	artifacts[6] = (struct orlix_tcti_target_artifact) {
		.name = "target_runtime_capability_cohort_artifact.def",
		.data = runtime_capability_cohort.data,
		.length = runtime_capability_cohort.length,
	};
	artifacts[7] = (struct orlix_tcti_target_artifact) {
		.name = "target_system_accessor_reconciliation.def",
		.data = system_accessors.data,
		.length = system_accessors.length,
	};
	if (fault && fault->stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_VALIDATION) {
		if (fault->validation_artifact >= ARRAY_SIZE(artifacts) ||
		    !artifacts[fault->validation_artifact].length) {
			error = ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT;
			errno = EINVAL;
			goto out;
		}
		/* Deterministically corrupt one emitted artifact so tests exercise
		 * the real validator rather than bypassing it synthetically. */
		((char *)artifacts[fault->validation_artifact].data)[0] = '\0';
	}
	if (validate_artifact_bundle(&manifest, &asl_availability,
				     &instruction_artifact, &feature_artifact,
				     &feature_field_domains,
				     &runtime_capability_cohort,
				     &register_artifact, &system_accessors)) {
		error = ORLIX_TCTI_TARGET_REFRESH_VALIDATION;
		errno = EINVAL;
		goto out;
	}
	if (orlix_tcti_target_artifact_publish(
		    canonical_root_fd, ORLIX_TCTI_TARGET_REFRESH_PUBLISH_NAME,
		    ORLIX_TCTI_TARGET_REFRESH_GENERATION_PREFIX, artifacts,
		    sizeof(artifacts) / sizeof(artifacts[0]), &provenance,
		    fault && fault->stage == ORLIX_TCTI_TARGET_REFRESH_FAULT_PUBLICATION ?
			    &fault->publication : NULL,
		    result ? &result->publish : &publish_result)) {
		error = ORLIX_TCTI_TARGET_REFRESH_PUBLISH;
		goto out;
	}

	/*
	 * The publisher validates existing generations before selection and
	 * returns immediately after its single atomic selector rename. Nothing
	 * after publication can manufacture a failure.
	 */
out:
	free(instructions.data);
	free(features.data);
	free(registers.data);
	free(manifest.data);
	free(asl_availability.data);
	free(instruction_artifact.data);
	free(feature_artifact.data);
	free(feature_field_domains.data);
	free(runtime_capability_cohort.data);
	free(register_artifact.data);
	free(system_accessors.data);
	set_result(result, error);
	return error == ORLIX_TCTI_TARGET_REFRESH_OK ? 0 : -1;
}

int orlix_tcti_target_refresh(
	int canonical_root_fd, const char *instructions_path,
	const char *features_path, const char *registers_path,
	struct orlix_tcti_target_refresh_result *result)
{
	return orlix_tcti_target_refresh_with_fault(
		canonical_root_fd, instructions_path, features_path, registers_path,
		NULL, result);
}

#ifndef ORLIX_TCTI_TARGET_REFRESH_NO_MAIN
int main(int argc, char **argv)
{
	struct orlix_tcti_target_refresh_result result;
	int canonical_fd;
	int status;

	if (argc != 5) {
		fprintf(stderr,
			"usage: %s CANONICAL_DIR Instructions.json Features.json Registers.json\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	canonical_fd = open(argv[1],
			    O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (canonical_fd < 0) {
		fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}
	status = orlix_tcti_target_refresh(canonical_fd, argv[2], argv[3],
					  argv[4], &result);
	close(canonical_fd);
	if (status) {
		fprintf(stderr, "OrlixTCTI ISA refresh: %s",
			orlix_tcti_target_refresh_error_name(result.error));
		if (result.publish.error != ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK)
			fprintf(stderr, ": %s",
				orlix_tcti_target_artifact_publish_error_name(
					result.publish.error));
		fputc('\n', stderr);
	}
	return status ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
