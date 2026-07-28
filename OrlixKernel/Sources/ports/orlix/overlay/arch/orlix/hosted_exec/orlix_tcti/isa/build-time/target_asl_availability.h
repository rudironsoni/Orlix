/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_ASL_AVAILABILITY_H
#define ORLIX_TCTI_TARGET_ASL_AVAILABILITY_H

#include <stddef.h>
#include <stdio.h>

#include "target_arm_xml_package.h"

/* This is source availability only. It is never semantic provenance. */
enum orlix_tcti_target_semantic_provenance_error {
	ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_OK = 0,
	ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_SOURCE,
	ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_MISSING_OPERATION,
	ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_IO,
};

enum orlix_tcti_target_semantic_provenance_error
orlix_tcti_target_semantic_provenance_emit(
	const char *source, size_t length,
	const struct orlix_tcti_arm_xml_package *package, FILE *output);

#endif /* ORLIX_TCTI_TARGET_ASL_AVAILABILITY_H */
