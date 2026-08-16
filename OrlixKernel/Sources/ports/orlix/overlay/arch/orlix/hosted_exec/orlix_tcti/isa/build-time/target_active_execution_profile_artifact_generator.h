/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_TARGET_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

enum orlix_tcti_active_execution_profile_artifact_generator_error {
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_OK,
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_INVALID_ARGUMENT,
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_UNKNOWN_FEATURE,
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_DUPLICATE_FEATURE,
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_UNPROMOTED_ENABLE,
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_SOURCE_MISMATCH,
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_COUNT_MISMATCH,
	ORLIX_TCTI_ACTIVE_EXECUTION_PROFILE_ARTIFACT_GENERATOR_IO,
};

enum orlix_tcti_active_execution_profile_artifact_generator_error
orlix_tcti_active_execution_profile_artifact_emit(const char *features,
	size_t features_length, const char *feature_applicability,
	size_t feature_applicability_length, const char *profile,
	size_t profile_length,
	const char *promotion_manifest, size_t promotion_manifest_length,
	const char *generator_schema, size_t generator_schema_length,
	const char *source_bound_proof, size_t source_bound_proof_length,
	const char *runtime_capability_cohort, size_t runtime_capability_cohort_length,
	const char *classification, size_t classification_length,
	const char *proof_registry, size_t proof_registry_length,
	const char *source_manifest, size_t source_manifest_length, FILE *output);

const char *orlix_tcti_active_execution_profile_artifact_generator_error_name(
	enum orlix_tcti_active_execution_profile_artifact_generator_error error);
#endif
