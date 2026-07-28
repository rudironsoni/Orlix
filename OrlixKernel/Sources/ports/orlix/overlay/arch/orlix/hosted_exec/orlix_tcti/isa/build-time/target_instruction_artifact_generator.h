/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

/*
 * Host-only refresh tool for the pinned Arm Instructions.json source.  The
 * emitted include is C data, not a runtime JSON dependency.  It retains every
 * imported A64 leaf and its symbolic source condition.  In particular, it
 * never consults the runtime HWCAP projection.
 */
enum orlix_tcti_target_instruction_artifact_generator_error {
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK = 0,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_PARSE,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_METADATA,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_DIGEST,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_CONDITION,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_IO,
};

/*
 * Emits a deterministic guarded C include to output.  On every source or
 * allocation failure it writes no bytes to output.
 */
enum orlix_tcti_target_instruction_artifact_generator_error
orlix_tcti_target_instruction_artifact_emit(const char *source, size_t length,
				      FILE *output);

enum orlix_tcti_target_instruction_artifact_generator_error
orlix_tcti_target_instruction_artifact_emit_expected(
	const char *source, size_t length, const char *expected_source_sha256,
	FILE *output);

const char *orlix_tcti_target_instruction_artifact_generator_error_name(
	enum orlix_tcti_target_instruction_artifact_generator_error error);

#endif /* ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_H */
