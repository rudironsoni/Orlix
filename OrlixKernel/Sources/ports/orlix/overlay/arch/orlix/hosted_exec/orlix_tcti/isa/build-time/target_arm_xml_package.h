/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_ARM_XML_PACKAGE_H
#define ORLIX_TCTI_TARGET_ARM_XML_PACKAGE_H

#include <stddef.h>

#define ORLIX_TCTI_ARM_XML_ARCHIVE_NAME \
	"ISA_A64_xml_A_profile-2026-06.tar.gz"
#define ORLIX_TCTI_ARM_XML_RELEASE_NAME \
	"ISA_A64_xml_A_profile-2026-06"
#define ORLIX_TCTI_ARM_XML_SOURCE_URL \
	"https://developer.arm.com/-/cdn-downloads/permalink/Exploration-Tools-A64-ISA/ISA_A64/ISA_A64_xml_A_profile-2026-06.tar.gz"
#define ORLIX_TCTI_ARM_XML_ARCHIVE_SHA256 \
	"63a01a1696483bbe2edfef9e0f0cd053d6c1c619ec0587876cb7a60bb344f354"
#define ORLIX_TCTI_ARM_XML_SHARED_SHA256 \
	"21edeadbc26408a35bcf7315bfbcbb8a69e9e6d86d952c80b153d273f0f2349f"
#define ORLIX_TCTI_ARM_XML_NOTICE_SHA256 \
	"9c2cc480e9706819e0d295329cf7f94f9256c610dfa9bc8f055a05389f74704a"
#define ORLIX_TCTI_ARM_XML_INDEX_SHA256 \
	"905dd3dd5537ff89514e21ce0d43bf180b84d2c45e5e498dfdcc502165185b93"
/* Proprietary Arm notice identity only. No notice or XML body is redistributed. */
#define ORLIX_TCTI_ARM_XML_LICENSE_CLASS \
	"proprietary_notice_hash_only_no_source_redistribution"

#define ORLIX_TCTI_ARM_XML_SHARED_PS_COUNT 1831U
#define ORLIX_TCTI_ARM_XML_SHARED_ANCHOR_COUNT 2656U
#define ORLIX_TCTI_ARM_XML_INDEX_FORM_COUNT 516U
#define ORLIX_TCTI_ARM_XML_RELEASE_FILE_COUNT 2316U
#define ORLIX_TCTI_ARM_XML_INSTRUCTION_FILE_COUNT 2299U
#define ORLIX_TCTI_ARM_XML_ENCODING_COUNT 4623U
#define ORLIX_TCTI_ARM_XML_INSTRUCTION_PS_COUNT 5518U
#define ORLIX_TCTI_ARM_XML_RELEASE_DIGEST \
	"bbe8309a4c746a996c84a3db7c92fa2a6a7453e6915231570f23e2d361bfdccd"

enum orlix_tcti_arm_xml_package_error {
	ORLIX_TCTI_ARM_XML_PACKAGE_OK = 0,
	ORLIX_TCTI_ARM_XML_PACKAGE_INVALID_ARGUMENT,
	ORLIX_TCTI_ARM_XML_PACKAGE_RELEASE_PATH,
	ORLIX_TCTI_ARM_XML_PACKAGE_IO,
	ORLIX_TCTI_ARM_XML_PACKAGE_FILE_SET,
	ORLIX_TCTI_ARM_XML_PACKAGE_IDENTITY,
	ORLIX_TCTI_ARM_XML_PACKAGE_MALFORMED_XML,
	ORLIX_TCTI_ARM_XML_PACKAGE_DUPLICATE_HELPER,
	ORLIX_TCTI_ARM_XML_PACKAGE_MISSING_HELPER,
	ORLIX_TCTI_ARM_XML_PACKAGE_INDEX,
};

struct orlix_tcti_arm_xml_package {
	size_t shared_ps_count;
	size_t shared_anchor_count;
	size_t index_form_count;
	struct orlix_tcti_arm_xml_semantic_entry *entries;
	size_t entry_count;
};

struct orlix_tcti_arm_xml_semantic_section {
	char *locator;
	size_t section_count;
	char normalized_sha256[65];
	size_t shared_helper_count;
	char shared_helpers_sha256[65];
};

struct orlix_tcti_arm_xml_semantic_entry {
	char *encoding_name;
	char *relative_file;
	struct orlix_tcti_arm_xml_semantic_section decode;
	struct orlix_tcti_arm_xml_semantic_section operation;
};

enum orlix_tcti_arm_xml_package_error
orlix_tcti_arm_xml_document_validate(const char *xml, size_t length);

enum orlix_tcti_arm_xml_package_error
orlix_tcti_arm_xml_package_validate(
	const char *archive_path, const char *release_path,
	struct orlix_tcti_arm_xml_package *package);

void orlix_tcti_arm_xml_package_destroy(
	struct orlix_tcti_arm_xml_package *package);

const struct orlix_tcti_arm_xml_semantic_entry *
orlix_tcti_arm_xml_package_entry(
	const struct orlix_tcti_arm_xml_package *package, const char *encoding_name);

const char *orlix_tcti_arm_xml_package_error_name(
	enum orlix_tcti_arm_xml_package_error error);

#endif /* ORLIX_TCTI_TARGET_ARM_XML_PACKAGE_H */
