/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_ARTIFACT_PUBLISHER_H
#define ORLIX_TCTI_TARGET_ARTIFACT_PUBLISHER_H

#include <stddef.h>

#define ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME 128U

/*
 * A host-only publisher for generated OrlixTCTI target artifacts.  Callers pass an
 * already-open, trusted ORLIX_BUILD_ROOT directory descriptor and one safe
 * child directory name.  The publisher owns only that child/{generation,current};
 * it never accepts a source-tree path or a caller-provided artifact path.
 */
struct orlix_tcti_target_artifact {
	const char *name;
	const void *data;
	size_t length;
};

/*
 * Every published generation identifies the C generator schema and the three
 * Arm inputs from which it was produced.  Values are copied into an immutable
 * manifest and included in the deterministic bundle digest.
 */
struct orlix_tcti_target_artifact_provenance {
	const char *schema;
	const char *generator;
	const char *instructions_sha256;
	const char *features_sha256;
	const char *registers_sha256;
};

/* Compute the standard SHA-256 used by immutable generation manifests. */
void orlix_tcti_target_artifact_sha256(const void *data, size_t length,
				 char digest[65]);

enum orlix_tcti_target_artifact_publish_error {
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OK = 0,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_INVALID_NAME,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_ROOT,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_CREATE_GENERATION,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_OPEN_GENERATION,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_TEMP_OPEN,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_WRITE,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_LOCK_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_FILE_CLOSE,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_RENAME,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_MANIFEST,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_GENERATION_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_GENERATION,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_LOCK_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_SELECTOR_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_PUBLISH_ALREADY_EXISTS,
};

enum orlix_tcti_target_artifact_publish_stage {
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_NONE = 0,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_TEMP_OPEN,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_WRITE,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_LOCK_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_FILE_CLOSE,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_RENAME,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_MANIFEST,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_GENERATION_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_GENERATION,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_LOCK_SYNC,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR,
	ORLIX_TCTI_TARGET_ARTIFACT_STAGE_SELECTOR_SYNC,
};

/*
 * Test-only deterministic fault injection.  Production callers pass NULL.
 * A failure before selector rename leaves the existing current selector
 * untouched.  SELECTOR_SYNC is deliberately post-publication: the selector
 * has already been atomically replaced and only durability is uncertain.
 */
struct orlix_tcti_target_artifact_publish_fault {
	enum orlix_tcti_target_artifact_publish_stage stage;
	/* Exercise checked short-write handling. Zero uses ordinary write sizes. */
	size_t maximum_write_bytes;
};

struct orlix_tcti_target_artifact_publish_result {
	enum orlix_tcti_target_artifact_publish_error error;
	int system_error;
	enum orlix_tcti_target_artifact_publish_stage stage;
};

int orlix_tcti_target_artifact_publish(
	int build_root_fd, const char *publish_name, const char *generation,
	const struct orlix_tcti_target_artifact *artifacts, size_t artifact_count,
	const struct orlix_tcti_target_artifact_provenance *provenance,
	const struct orlix_tcti_target_artifact_publish_fault *fault,
	struct orlix_tcti_target_artifact_publish_result *result);

const char *orlix_tcti_target_artifact_publish_error_name(
	enum orlix_tcti_target_artifact_publish_error error);

enum orlix_tcti_target_artifact_verify_error {
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_OK = 0,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_OPEN_ROOT,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_SELECTOR_OPEN,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_SELECTOR_FORMAT,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_GENERATION_OPEN,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_MANIFEST_OPEN,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_MANIFEST_FORMAT,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_PROVENANCE,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_BUNDLE_DIGEST,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_OPEN,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_TYPE,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_SIZE,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_ARTIFACT_DIGEST,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_EXTRA_ARTIFACT,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_RESOURCE_LIMIT,
	ORLIX_TCTI_TARGET_ARTIFACT_VERIFY_IO,
};

struct orlix_tcti_target_artifact_verify_result {
	enum orlix_tcti_target_artifact_verify_error error;
	int system_error;
	char artifact[ORLIX_TCTI_TARGET_ARTIFACT_MAX_NAME + 1];
};

/*
 * Verifies the atomically selected generation without following filesystem
 * links. The expected provenance is an input contract, not data trusted from
 * the manifest.
 */
int orlix_tcti_target_artifact_verify(
	int build_root_fd, const char *publish_name,
	const struct orlix_tcti_target_artifact_provenance *expected_provenance,
	struct orlix_tcti_target_artifact_verify_result *result);

const char *orlix_tcti_target_artifact_verify_error_name(
	enum orlix_tcti_target_artifact_verify_error error);

#endif /* ORLIX_TCTI_TARGET_ARTIFACT_PUBLISHER_H */
