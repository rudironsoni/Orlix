/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_TARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

/*
 * This emitter preserves structural feature references from every Instruction
 * condition and every owned operand condition.  A reference is a candidate,
 * never an applicability, implementation, or proof claim.
 */
enum tcti_runtime_capability_cohort_artifact_generator_error {
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_OK,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INVALID_ARGUMENT,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INSTRUCTION_IMPORT,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_FEATURE_IMPORT,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_UNKNOWN_FEATURE,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_INVALID_EXPRESSION,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_COUNT_MISMATCH,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_NO_MEMORY,
	TCTI_RUNTIME_CAPABILITY_COHORT_ARTIFACT_GENERATOR_IO,
};

enum tcti_runtime_capability_cohort_artifact_generator_error
tcti_runtime_capability_cohort_artifact_emit(const char *instructions,
	size_t instructions_length, const char *features, size_t features_length,
	FILE *output);

const char *tcti_runtime_capability_cohort_artifact_generator_error_name(
	enum tcti_runtime_capability_cohort_artifact_generator_error error);

#endif
