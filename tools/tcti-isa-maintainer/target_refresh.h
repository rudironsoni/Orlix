/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_REFRESH_H
#define ORLIX_TCTI_TARGET_REFRESH_H

#include "target_artifact_publisher.h"

enum tcti_target_refresh_error {
	TCTI_TARGET_REFRESH_OK = 0,
	TCTI_TARGET_REFRESH_INVALID_ARGUMENT,
	TCTI_TARGET_REFRESH_SOURCE_IO,
	TCTI_TARGET_REFRESH_SOURCE_LIMIT,
	TCTI_TARGET_REFRESH_MANIFEST,
	TCTI_TARGET_REFRESH_ASL_AVAILABILITY,
	TCTI_TARGET_REFRESH_INSTRUCTIONS,
	TCTI_TARGET_REFRESH_FEATURES,
	TCTI_TARGET_REFRESH_REGISTERS,
	TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS,
	TCTI_TARGET_REFRESH_CAPTURE,
	TCTI_TARGET_REFRESH_PUBLISH,
	TCTI_TARGET_REFRESH_VERIFY,
};

struct tcti_target_refresh_result {
	enum tcti_target_refresh_error error;
	struct tcti_target_artifact_publish_result publish;
	struct tcti_target_artifact_verify_result verify;
};

/*
 * Import each pinned Arm source in memory, emit all source-derived C artifacts
 * before publication, then atomically publish and verify one immutable bundle
 * below the trusted build-root descriptor.  No source-tree path is writable
 * through this API.
 */
int tcti_target_refresh(int build_root_fd, const char *instructions_path,
			const char *features_path, const char *registers_path,
			struct tcti_target_refresh_result *result);

const char *tcti_target_refresh_error_name(enum tcti_target_refresh_error error);

#endif /* ORLIX_TCTI_TARGET_REFRESH_H */
