/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_MANIFEST_GENERATOR_H
#define ORLIX_TCTI_TARGET_MANIFEST_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

enum target_manifest_generator_error {
	TCTI_TARGET_MANIFEST_GENERATOR_OK,
	TCTI_TARGET_MANIFEST_GENERATOR_PARSE,
	TCTI_TARGET_MANIFEST_GENERATOR_METADATA,
	TCTI_TARGET_MANIFEST_GENERATOR_COUNT,
	TCTI_TARGET_MANIFEST_GENERATOR_DIGEST,
	TCTI_TARGET_MANIFEST_GENERATOR_IO,
	TCTI_TARGET_MANIFEST_GENERATOR_MANIFEST_MISMATCH,
};

/*
 * Emit the complete source-derived manifest only after the pinned
 * Instructions.json source, metadata, leaf count, and digest all validate.
 */
enum target_manifest_generator_error
target_manifest_generator_emit(const char *source, size_t length, FILE *output);

#endif /* ORLIX_TCTI_TARGET_MANIFEST_GENERATOR_H */
