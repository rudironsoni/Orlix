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
enum orlix_tcti_target_instruction_artifact_error {
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OK = 0,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_METADATA,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_DIGEST,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_NO_MEMORY,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_IO,
};

/*
 * Emits a deterministic guarded C include to output.  On every source or
 * allocation failure it writes no bytes to output.
 */
enum orlix_tcti_target_instruction_artifact_error
orlix_tcti_target_instruction_artifact_emit(const char *source, size_t length,
				      FILE *output);

const char *orlix_tcti_target_instruction_artifact_error_name(
	enum orlix_tcti_target_instruction_artifact_error error);

#endif /* ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_H */
