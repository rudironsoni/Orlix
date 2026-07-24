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
enum tcti_target_instruction_artifact_error {
	TCTI_TARGET_INSTRUCTION_ARTIFACT_OK = 0,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_METADATA,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_DIGEST,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_NO_MEMORY,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_IO,
};

/*
 * Emits a deterministic guarded C include to output.  On every source or
 * allocation failure it writes no bytes to output.
 */
enum tcti_target_instruction_artifact_error
tcti_target_instruction_artifact_emit(const char *source, size_t length,
				      FILE *output);

const char *tcti_target_instruction_artifact_error_name(
	enum tcti_target_instruction_artifact_error error);

#endif /* ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_H */
