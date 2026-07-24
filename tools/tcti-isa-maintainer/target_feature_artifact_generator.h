/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_TARGET_FEATURE_ARTIFACT_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

#define TCTI_FEATURE_ARTIFACT_MAX_INPUT (8U * 1024U * 1024U)
#define TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH 1243621U

/*
 * This host-maintenance emitter is the only Features.json consumer in this
 * path.  Its output is a deterministic .def file for C consumers.  The source
 * macro includes the exact source length for validating retained byte spans.
 * It keeps the imported AST intact rather than lowering a condition into the
 * current Linux HWCAP projection.
 */
enum tcti_feature_artifact_error {
	TCTI_FEATURE_ARTIFACT_OK,
	TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT,
	TCTI_FEATURE_ARTIFACT_INVALID_SOURCE,
	TCTI_FEATURE_ARTIFACT_UNSUPPORTED_GRAMMAR,
	TCTI_FEATURE_ARTIFACT_LIMIT,
	TCTI_FEATURE_ARTIFACT_NO_MEMORY,
	TCTI_FEATURE_ARTIFACT_PIN_MISMATCH,
	TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH,
	TCTI_FEATURE_ARTIFACT_IO,
	TCTI_FEATURE_ARTIFACT_MISMATCH,
};

/*
 * Emits no bytes until target_feature_model has accepted the complete pinned
 * source.  Callers that need an atomic replacement must write to a temporary
 * file and rename it only after this function returns TCTI_FEATURE_ARTIFACT_OK.
 */
enum tcti_feature_artifact_error tcti_target_feature_artifact_emit(
	const char *source, size_t length, FILE *output);

const char *tcti_target_feature_artifact_error_name(
	enum tcti_feature_artifact_error error);

#endif
