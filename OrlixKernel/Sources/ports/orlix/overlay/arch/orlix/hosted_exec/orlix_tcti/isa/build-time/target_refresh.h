/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_REFRESH_H
#define ORLIX_TCTI_TARGET_REFRESH_H

#include "target_artifact_publisher.h"
#include "target_arm_xml_package.h"
#include "target_classification_generator.h"

enum orlix_tcti_target_refresh_error {
	ORLIX_TCTI_TARGET_REFRESH_OK = 0,
	ORLIX_TCTI_TARGET_REFRESH_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_REFRESH_SOURCE_IO,
	ORLIX_TCTI_TARGET_REFRESH_SOURCE_LIMIT,
	ORLIX_TCTI_TARGET_REFRESH_SOURCE_IDENTITY,
	ORLIX_TCTI_TARGET_REFRESH_ARM_XML_PACKAGE,
	ORLIX_TCTI_TARGET_REFRESH_PARSE,
	ORLIX_TCTI_TARGET_REFRESH_VALIDATION,
	ORLIX_TCTI_TARGET_REFRESH_MANIFEST,
	ORLIX_TCTI_TARGET_REFRESH_CLASSIFICATION,
	ORLIX_TCTI_TARGET_REFRESH_SEMANTIC_PROVENANCE,
	ORLIX_TCTI_TARGET_REFRESH_INSTRUCTIONS,
	ORLIX_TCTI_TARGET_REFRESH_FEATURES,
	ORLIX_TCTI_TARGET_REFRESH_FEATURE_FIELD_DOMAINS,
	ORLIX_TCTI_TARGET_REFRESH_FEATURE_APPLICABILITY,
	ORLIX_TCTI_TARGET_REFRESH_RUNTIME_CAPABILITY_COHORT,
	ORLIX_TCTI_TARGET_REFRESH_REGISTERS,
	ORLIX_TCTI_TARGET_REFRESH_SYSTEM_ACCESSORS,
	ORLIX_TCTI_TARGET_REFRESH_PUBLISH,
};

struct orlix_tcti_target_refresh_result {
	enum orlix_tcti_target_refresh_error error;
	enum orlix_tcti_arm_xml_package_error arm_xml_error;
	enum orlix_tcti_target_classification_error classification_error;
	struct orlix_tcti_target_artifact_publish_result publish;
};

enum orlix_tcti_target_refresh_fault_stage {
	ORLIX_TCTI_TARGET_REFRESH_FAULT_NONE = 0,
	ORLIX_TCTI_TARGET_REFRESH_FAULT_PARSE,
	ORLIX_TCTI_TARGET_REFRESH_FAULT_VALIDATION,
	ORLIX_TCTI_TARGET_REFRESH_FAULT_PUBLICATION,
};

struct orlix_tcti_target_refresh_fault {
	enum orlix_tcti_target_refresh_fault_stage stage;
	size_t validation_artifact;
	struct orlix_tcti_target_artifact_publish_fault publication;
};

/*
 * Import each pinned Arm source in memory, emit all source-derived C artifacts
 * before publication, then atomically select one immutable, full-bundle-
 * addressed generation below the authoritative ISA source-tree descriptor.
 */
int orlix_tcti_target_refresh(int canonical_root_fd,
			      const char *instructions_path,
			      const char *features_path, const char *registers_path,
			      const char *arm_xml_archive_path,
			      const char *arm_xml_release_path,
			struct orlix_tcti_target_refresh_result *result);

/* Test-only deterministic publication fault injection. Production uses the
 * wrapper above, which always passes a NULL fault. */
int orlix_tcti_target_refresh_with_fault(int canonical_root_fd,
				 const char *instructions_path,
				 const char *features_path, const char *registers_path,
				 const char *arm_xml_archive_path,
				 const char *arm_xml_release_path,
					 const struct orlix_tcti_target_refresh_fault *fault,
			struct orlix_tcti_target_refresh_result *result);

const char *orlix_tcti_target_refresh_error_name(enum orlix_tcti_target_refresh_error error);

#endif /* ORLIX_TCTI_TARGET_REFRESH_H */
