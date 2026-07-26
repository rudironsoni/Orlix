/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_REFRESH_H
#define ORLIX_TCTI_TARGET_REFRESH_H

#include "target_artifact_publisher.h"

enum orlix_tcti_target_refresh_error {
	ORLIX_TCTI_TARGET_REFRESH_OK = 0,
	ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO,
	ORLIX_TCTI_TARGET_REFRESH_SOURCE_LIMIT,
	ORLIX_TCTI_TARGET_REFRESH_MANIFEST,
	ORLIX_TCTI_TARGET_REFRESH_ASL_AVAILABILITY,
	ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS,
	ORLIX_TCTI_TARGET_REFRESH_FEATURES,
	ORLIX_TCTI_TARGET_REFRESH_FEATURE_FIELD_DOMAINS,
	ORLIX_TCTI_TARGET_REFRESH_RUNTIME_CAPABILITY_COHORT,
	ORLIX_TCTI_TARGET_REFRESH_REGISTERS,
	ORLIX_TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS,
	ORLIX_TCTI_TARGET_REFRESH_CAPTURE,
	ORLIX_TCTI_TARGET_REFRESH_CANONICAL_MISSING,
	ORLIX_TCTI_TARGET_REFRESH_CANONICAL_IO,
	ORLIX_TCTI_TARGET_REFRESH_CANONICAL_MISMATCH,
	ORLIX_TCTI_TARGET_REFRESH_PUBLISH,
	ORLIX_TCTI_TARGET_REFRESH_VERIFY,
};

struct orlix_tcti_target_refresh_result {
	enum orlix_tcti_target_refresh_error error;
	/* Set only when a checked source-derived artifact blocks publication. */
	const char *canonical_artifact;
	struct orlix_tcti_target_artifact_publish_result publish;
	struct orlix_tcti_target_artifact_verify_result verify;
};

/*
 * Import each pinned Arm source in memory, emit all source-derived C artifacts
 * before publication, compare every source-derived artifact with its checked
 * canonical counterpart, then atomically publish and verify one immutable
 * bundle below the trusted build-root descriptor.  Both descriptors are read
 * only.  No source-tree path is writable through this API.
 */
int orlix_tcti_target_refresh(int build_root_fd, int canonical_root_fd,
			const char *instructions_path,
			const char *features_path, const char *registers_path,
			struct orlix_tcti_target_refresh_result *result);

const char *orlix_tcti_target_refresh_error_name(enum orlix_tcti_target_refresh_error error);

#endif /* ORLIX_TCTI_TARGET_REFRESH_H */
