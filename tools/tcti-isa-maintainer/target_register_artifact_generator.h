/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_REGISTER_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_TARGET_REGISTER_ARTIFACT_GENERATOR_H

#include "target_register_model.h"

#include <stddef.h>
#include <stdio.h>

#define TCTI_REGISTER_ARTIFACT_MAX_INPUT (128U * 1024U * 1024U)

enum tcti_register_artifact_error {
	TCTI_REGISTER_ARTIFACT_OK,
	TCTI_REGISTER_ARTIFACT_INVALID_ARGUMENT,
	TCTI_REGISTER_ARTIFACT_INVALID_SOURCE,
	TCTI_REGISTER_ARTIFACT_UNSUPPORTED_GRAMMAR,
	TCTI_REGISTER_ARTIFACT_LIMIT,
	TCTI_REGISTER_ARTIFACT_NO_MEMORY,
	TCTI_REGISTER_ARTIFACT_PIN_MISMATCH,
	TCTI_REGISTER_ARTIFACT_COUNT_MISMATCH,
	TCTI_REGISTER_ARTIFACT_UNRESOLVED_MODEL,
	TCTI_REGISTER_ARTIFACT_IO,
	TCTI_REGISTER_ARTIFACT_MISMATCH,
};

/*
 * Validate a populated typed model and emit its deterministic C macro
 * artifact. Validation completes before the first output byte is written.
 * This entry point exists so focused tests can prove that an unresolved model
 * is rejected without reparsing the pinned source.
 */
enum tcti_register_artifact_error
tcti_target_register_artifact_emit_model(
	const struct tcti_register_model *model, size_t source_length, FILE *output);

/*
 * Import the exact pinned Registers.json through target_register_model, then
 * emit the artifact. JSON remains an explicit refresh and audit input only.
 */
enum tcti_register_artifact_error tcti_target_register_artifact_emit(
	const char *source, size_t length, FILE *output);

const char *tcti_target_register_artifact_error_name(
	enum tcti_register_artifact_error error);

#endif
