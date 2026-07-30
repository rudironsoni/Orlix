/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_ARTIFACT_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

int orlix_tcti_runtime_feature_condition_artifact_emit(const char *instructions,
	size_t instructions_length, const char *features, size_t features_length,
	FILE *output);

#endif
