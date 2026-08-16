/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_CLASSIFICATION_GENERATOR_H
#define ORLIX_TCTI_TARGET_CLASSIFICATION_GENERATOR_H

#include <stddef.h>

enum orlix_tcti_target_classification_error {
	ORLIX_TCTI_TARGET_CLASSIFICATION_OK = 0,
	ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_CLASSIFICATION_ROW_COUNT,
	ORLIX_TCTI_TARGET_CLASSIFICATION_PROOF_REGISTRY,
	ORLIX_TCTI_TARGET_CLASSIFICATION_SOURCE_ROW,
	ORLIX_TCTI_TARGET_CLASSIFICATION_DUPLICATE_NAME,
	ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_ENTRY,
	ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_CANONICAL,
	ORLIX_TCTI_TARGET_CLASSIFICATION_PROOF_REFERENCE,
	ORLIX_TCTI_TARGET_CLASSIFICATION_OUTPUT,
};

struct orlix_tcti_target_classification_output {
	char *data;
	size_t length;
	enum orlix_tcti_target_classification_error error;
};

int orlix_tcti_target_classification_generate(int canonical_root_fd,
	struct orlix_tcti_target_classification_output *output);
int orlix_tcti_target_classification_matches(int canonical_root_fd,
	const void *data, size_t length);
void orlix_tcti_target_classification_destroy(
	struct orlix_tcti_target_classification_output *output);
const char *orlix_tcti_target_classification_error_name(
	enum orlix_tcti_target_classification_error error);

#endif /* ORLIX_TCTI_TARGET_CLASSIFICATION_GENERATOR_H */
