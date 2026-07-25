/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_ASL_AVAILABILITY_H
#define ORLIX_TCTI_TARGET_ASL_AVAILABILITY_H

#include <stddef.h>
#include <stdio.h>

/* This is source availability only. It is never semantic provenance. */
enum tcti_target_asl_availability_error {
	TCTI_TARGET_ASL_AVAILABILITY_OK = 0,
	TCTI_TARGET_ASL_AVAILABILITY_INVALID_ARGUMENT,
	TCTI_TARGET_ASL_AVAILABILITY_SOURCE,
	TCTI_TARGET_ASL_AVAILABILITY_MISSING_OPERATION,
	TCTI_TARGET_ASL_AVAILABILITY_IO,
};

enum tcti_target_asl_availability_error
tcti_target_asl_availability_emit(const char *source, size_t length,
				  FILE *output);

#endif /* ORLIX_TCTI_TARGET_ASL_AVAILABILITY_H */
