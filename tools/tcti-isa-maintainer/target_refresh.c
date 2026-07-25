/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include "target_asl_availability.h"
#include "target_feature_artifact_generator.h"
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

#define TCTI_TARGET_REFRESH_MAX_SOURCE (128U * 1024U * 1024U)
#define TCTI_TARGET_REFRESH_PUBLISH_NAME "aarchmrs-2026-06"
#define TCTI_TARGET_REFRESH_SCHEMA "tcti-aarchmrs-source-v1"
#define TCTI_TARGET_REFRESH_GENERATOR "tcti-isa-maintainer"

struct source_bytes {
	char *data;
	size_t length;
};

struct artifact_bytes {
	char *data;
	size_t length;
};

static void set_result(struct tcti_target_refresh_result *result,
			       enum tcti_target_refresh_error error)
{
	if (result)
		result->error = error;
}

const char *tcti_target_refresh_error_name(enum tcti_target_refresh_error error)
{
	switch (error) {
	case TCTI_TARGET_REFRESH_OK: return "success";
	case TCTI_TARGET_REFRESH_INVALID_ARGUMENT: return "invalid argument";
	case TCTI_TARGET_REFRESH_SOURCE_IO: return "source I/O failure";
	case TCTI_TARGET_REFRESH_SOURCE_LIMIT: return "source exceeds refresh limit";
	case TCTI_TARGET_REFRESH_MANIFEST: return "manifest generation failed";
	case TCTI_TARGET_REFRESH_ASL_AVAILABILITY: return "ASL availability generation failed";
	case TCTI_TARGET_REFRESH_INSTRUCTIONS: return "instruction artifact generation failed";
	case TCTI_TARGET_REFRESH_FEATURES: return "feature artifact generation failed";
	case TCTI_TARGET_REFRESH_REGISTERS: return "register artifact generation failed";
	case TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS: return "system accessor reconciliation failed";
	case TCTI_TARGET_REFRESH_CAPTURE: return "generated artifact capture failed";
	case TCTI_TARGET_REFRESH_CANONICAL_MISSING: return "checked canonical artifact is missing";
	case TCTI_TARGET_REFRESH_CANONICAL_IO: return "checked canonical artifact cannot be read";
	case TCTI_TARGET_REFRESH_CANONICAL_MISMATCH: return "generated artifact differs from checked canonical artifact";
	case TCTI_TARGET_REFRESH_PUBLISH: return "transactional publication failed";
	case TCTI_TARGET_REFRESH_VERIFY: return "published generation verification failed";
	}
	return "unknown refresh failure";
}

static int read_source(const char *path, struct source_bytes *source,
		       enum tcti_target_refresh_error *error)
{
	FILE *file;
	long length;
	size_t count;

	file = fopen(path, "rb");
	if (!file) {
		*error = TCTI_TARGET_REFRESH_SOURCE_IO;
		return -1;
	}
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0) {
		*error = TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > TCTI_TARGET_REFRESH_MAX_SOURCE) {
		*error = TCTI_TARGET_REFRESH_SOURCE_LIMIT;
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > SIZE_MAX - 1U || fseek(file, 0, SEEK_SET)) {
		*error = TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	source->data = malloc((size_t)length + 1U);
	if (!source->data) {
		*error = TCTI_TARGET_REFRESH_SOURCE_IO;
		fclose(file);
		return -1;
	}
	count = fread(source->data, 1, (size_t)length, file);
	if (count != (size_t)length || fclose(file)) {
		free(source->data);
		*source = (struct source_bytes) { 0 };
		*error = TCTI_TARGET_REFRESH_SOURCE_IO;
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
					output) == TCTI_TARGET_MANIFEST_GENERATOR_OK &&
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
	result = tcti_target_asl_availability_emit(source->data, source->length,
			output) == TCTI_TARGET_ASL_AVAILABILITY_OK &&
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
	result = tcti_target_instruction_artifact_emit(source->data, source->length,
			output) == TCTI_TARGET_INSTRUCTION_ARTIFACT_OK &&
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
	result = tcti_target_feature_artifact_emit(source->data, source->length,
			output) == TCTI_FEATURE_ARTIFACT_OK &&
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
	result = tcti_target_register_artifact_emit(source->data, source->length,
			output) == TCTI_REGISTER_ARTIFACT_OK &&
		!capture(output, artifact) ? 0 : -1;
	fclose(output);
	return result;
}

static int emit_system_accessors(const struct source_bytes *source,
				 struct artifact_bytes *artifact)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error import_error = { 0 };
	FILE *output = tmpfile();
	int result = -1;

	if (!output)
		return -1;
	if (!tcti_register_model_import(source->data, source->length, &model,
					&import_error) &&
	    tcti_system_accessor_reconciliation_emit(&model, output) ==
		TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK && !capture(output, artifact))
		result = 0;
	tcti_register_model_destroy(&model);
	fclose(output);
	return result;
}

static int compare_canonical_artifact(int canonical_root_fd,
				      const struct tcti_target_artifact *artifact,
				      enum tcti_target_refresh_error *error)
{
	char buffer[4096];
	struct stat status;
	size_t offset = 0;
	int fd;

	fd = openat(canonical_root_fd, artifact->name,
		    O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0) {
		*error = errno == ENOENT ? TCTI_TARGET_REFRESH_CANONICAL_MISSING :
			TCTI_TARGET_REFRESH_CANONICAL_IO;
		return -1;
	}
	if (fstat(fd, &status) || !S_ISREG(status.st_mode) || status.st_size < 0 ||
	    (uintmax_t)status.st_size != artifact->length) {
		close(fd);
		*error = TCTI_TARGET_REFRESH_CANONICAL_MISMATCH;
		return -1;
	}
	while (offset < artifact->length) {
		size_t remaining = artifact->length - offset;
		size_t wanted = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
		ssize_t count = read(fd, buffer, wanted);

		if (count < 0) {
			close(fd);
			*error = TCTI_TARGET_REFRESH_CANONICAL_IO;
			return -1;
		}
		if (!count ||
		    memcmp(buffer, (const unsigned char *)artifact->data + offset,
			   (size_t)count)) {
			close(fd);
			*error = TCTI_TARGET_REFRESH_CANONICAL_MISMATCH;
			return -1;
		}
		offset += (size_t)count;
	}
	if (close(fd)) {
		*error = TCTI_TARGET_REFRESH_CANONICAL_IO;
		return -1;
	}
	return 0;
}

int tcti_target_refresh(int build_root_fd, int canonical_root_fd,
			const char *instructions_path,
			const char *features_path, const char *registers_path,
			struct tcti_target_refresh_result *result)
{
	struct source_bytes instructions = { 0 }, features = { 0 }, registers = { 0 };
	struct artifact_bytes manifest = { 0 }, asl_availability = { 0 };
	struct artifact_bytes instruction_artifact = { 0 };
	struct artifact_bytes feature_artifact = { 0 }, register_artifact = { 0 };
	struct artifact_bytes system_accessors = { 0 };
	struct tcti_target_artifact artifacts[6];
	struct tcti_target_artifact_provenance provenance = {
		.schema = TCTI_TARGET_REFRESH_SCHEMA,
		.generator = TCTI_TARGET_REFRESH_GENERATOR,
	};
	struct tcti_target_artifact_publish_result publish_result = { 0 };
	struct tcti_target_artifact_verify_result verify_result = { 0 };
	char instruction_digest[65];
	char feature_digest[65];
	char register_digest[65];
	char generation[128];
	enum tcti_target_refresh_error error = TCTI_TARGET_REFRESH_OK;
	int published;

	if (result)
		*result = (struct tcti_target_refresh_result) { 0 };
	if (build_root_fd < 0 || canonical_root_fd < 0 || !instructions_path || !features_path ||
	    !registers_path) {
		set_result(result, TCTI_TARGET_REFRESH_INVALID_ARGUMENT);
		errno = EINVAL;
		return -1;
	}
	if (read_source(instructions_path, &instructions, &error) ||
	    read_source(features_path, &features, &error) ||
	    read_source(registers_path, &registers, &error))
		goto out;
	tcti_target_artifact_sha256(instructions.data, instructions.length,
				    instruction_digest);
	tcti_target_artifact_sha256(features.data, features.length, feature_digest);
	tcti_target_artifact_sha256(registers.data, registers.length, register_digest);
	provenance.instructions_sha256 = instruction_digest;
	provenance.features_sha256 = feature_digest;
	provenance.registers_sha256 = register_digest;
	if (emit_manifest(&instructions, &manifest)) {
		error = TCTI_TARGET_REFRESH_MANIFEST;
		goto out;
	}
	if (emit_asl_availability(&instructions, &asl_availability)) {
		error = TCTI_TARGET_REFRESH_ASL_AVAILABILITY;
		goto out;
	}
	if (emit_instructions(&instructions, &instruction_artifact)) {
		error = TCTI_TARGET_REFRESH_INSTRUCTIONS;
		goto out;
	}
	if (emit_features(&features, &feature_artifact)) {
		error = TCTI_TARGET_REFRESH_FEATURES;
		goto out;
	}
	if (emit_registers(&registers, &register_artifact)) {
		error = TCTI_TARGET_REFRESH_REGISTERS;
		goto out;
	}
	if (emit_system_accessors(&registers, &system_accessors)) {
		error = TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS;
		goto out;
	}
	if (snprintf(generation, sizeof(generation), "aarchmrs-2026-06-%.12s-%.12s-%.12s",
		     instruction_digest, feature_digest, register_digest) >=
	    (int)sizeof(generation)) {
		error = TCTI_TARGET_REFRESH_CAPTURE;
		goto out;
	}
	artifacts[0] = (struct tcti_target_artifact) {
		.name = "source_manifest.def", .data = manifest.data, .length = manifest.length,
	};
	artifacts[1] = (struct tcti_target_artifact) {
		.name = "target_asl_availability.def", .data = asl_availability.data,
		.length = asl_availability.length,
	};
	artifacts[2] = (struct tcti_target_artifact) {
		.name = "target_feature_artifact.def", .data = feature_artifact.data,
		.length = feature_artifact.length,
	};
	artifacts[3] = (struct tcti_target_artifact) {
		.name = "target_instruction_artifact_generated.h",
		.data = instruction_artifact.data, .length = instruction_artifact.length,
	};
	artifacts[4] = (struct tcti_target_artifact) {
		.name = "target_register_artifact.def", .data = register_artifact.data,
		.length = register_artifact.length,
	};
	artifacts[5] = (struct tcti_target_artifact) {
		.name = "target_system_accessor_reconciliation.def",
		.data = system_accessors.data, .length = system_accessors.length,
	};
	for (size_t index = 0; index < sizeof(artifacts) / sizeof(artifacts[0]); index++) {
		if (compare_canonical_artifact(canonical_root_fd, &artifacts[index],
					       &error)) {
			if (result)
				result->canonical_artifact = artifacts[index].name;
			goto out;
		}
	}
	published = tcti_target_artifact_publish(build_root_fd,
		TCTI_TARGET_REFRESH_PUBLISH_NAME, generation, artifacts,
		sizeof(artifacts) / sizeof(artifacts[0]), &provenance, NULL,
		result ? &result->publish : &publish_result);
	if (published && ((result ? result->publish.error : publish_result.error) !=
			TCTI_TARGET_ARTIFACT_PUBLISH_ALREADY_EXISTS)) {
		error = TCTI_TARGET_REFRESH_PUBLISH;
		goto out;
	}
	if (tcti_target_artifact_verify(build_root_fd, TCTI_TARGET_REFRESH_PUBLISH_NAME,
					&provenance,
					result ? &result->verify : &verify_result)) {
		error = TCTI_TARGET_REFRESH_VERIFY;
		goto out;
	}
out:
	free(instructions.data);
	free(features.data);
	free(registers.data);
	free(manifest.data);
	free(asl_availability.data);
	free(instruction_artifact.data);
	free(feature_artifact.data);
	free(register_artifact.data);
	free(system_accessors.data);
	set_result(result, error);
	return error == TCTI_TARGET_REFRESH_OK ? 0 : -1;
}

#ifndef TCTI_TARGET_REFRESH_NO_MAIN
int main(int argc, char **argv)
{
	struct tcti_target_refresh_result result;
	int root_fd;
	int status;

	if (argc != 6) {
		fprintf(stderr, "usage: %s BUILD_ROOT CANONICAL_DIR Instructions.json Features.json Registers.json\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	root_fd = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (root_fd < 0) {
		fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}
	int canonical_fd = open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (canonical_fd < 0) {
		fprintf(stderr, "%s: %s\n", argv[2], strerror(errno));
		close(root_fd);
		return EXIT_FAILURE;
	}
	status = tcti_target_refresh(root_fd, canonical_fd, argv[3], argv[4], argv[5],
				     &result);
	close(canonical_fd);
	close(root_fd);
	if (status) {
		fprintf(stderr, "tcti ISA refresh: %s",
			tcti_target_refresh_error_name(result.error));
		if (result.canonical_artifact)
			fprintf(stderr, ": %s", result.canonical_artifact);
		fputc('\n', stderr);
	}
	return status ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
