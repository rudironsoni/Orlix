/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_TARGET_FEATURE_ARTIFACT_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

#define ORLIX_TCTI_FEATURE_ARTIFACT_MAX_INPUT (8U * 1024U * 1024U)
#define ORLIX_TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH 1243621U

/*
 * This host-maintenance emitter is the only Features.json consumer in this
 * path.  Its output is a deterministic .def file for C consumers.  The source
 * macro includes the exact source length for validating retained byte spans.
 * It keeps the imported AST intact rather than lowering a condition into the
 * current Linux HWCAP projection.
 */
enum orlix_tcti_feature_artifact_error {
	ORLIX_TCTI_FEATURE_ARTIFACT_OK,
	ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT,
	ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_SOURCE,
	ORLIX_TCTI_FEATURE_ARTIFACT_UNSUPPORTED_GRAMMAR,
	ORLIX_TCTI_FEATURE_ARTIFACT_LIMIT,
	ORLIX_TCTI_FEATURE_ARTIFACT_NO_MEMORY,
	ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH,
	ORLIX_TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH,
	ORLIX_TCTI_FEATURE_ARTIFACT_IO,
	ORLIX_TCTI_FEATURE_ARTIFACT_MISMATCH,
};

/*
 * Emits no bytes until target_feature_model has accepted the complete pinned
 * source.  Callers that need an atomic replacement must write to a temporary
 * file and rename it only after this function returns ORLIX_TCTI_FEATURE_ARTIFACT_OK.
 */
enum orlix_tcti_feature_artifact_error orlix_tcti_target_feature_artifact_emit(
	const char *source, size_t length, FILE *output);

const char *orlix_tcti_target_feature_artifact_error_name(
	enum orlix_tcti_feature_artifact_error error);

#endif
