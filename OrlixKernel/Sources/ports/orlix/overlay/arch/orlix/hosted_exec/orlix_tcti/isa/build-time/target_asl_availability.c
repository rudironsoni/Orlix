/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_asl_availability.h"

#include "target_inventory_import.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ORLIX_TCTI_TARGET_LEAF_COUNT 4350U
#define ORLIX_TCTI_DDI0602_PROVENANCE_COUNT 4332U
#define ORLIX_TCTI_NOT_SPECIFIED_PROVENANCE_COUNT 18U

static const char not_specified_body[] = "// Not specified";

static int emit_string(FILE *output, const char *text)
{
	return fprintf(output, "\"%s\"", text) < 0 ? -1 : 0;
}

static int valid_sha256(const char digest[65])
{
	size_t index;

	if (!digest || digest[64] != '\0')
		return 0;
	for (index = 0; index < 64U; index++) {
		const char character = digest[index];

		if (!((character >= '0' && character <= '9') ||
		      (character >= 'a' && character <= 'f')))
			return 0;
	}
	return 1;
}

static const struct orlix_tcti_target_operation *semantic_operation(
	const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_target_operation *operation)
{
	if (!operation)
		return NULL;
	if (!operation->is_alias)
		return operation;
	if (!operation->canonical_operation_id)
		return NULL;
	return orlix_tcti_target_inventory_operation(
		inventory, operation->canonical_operation_id);
}

static int official_semantics_not_specified_ordinal(size_t ordinal)
{
	static const uint16_t ordinals[] = {
		2235U, 2302U, 2308U, 2309U, 2310U, 2311U,
		2675U, 2676U, 2677U, 2678U, 2679U, 2680U,
		2681U, 2682U, 2683U, 2684U, 2685U, 2686U,
	};
	size_t index;

	for (index = 0; index < sizeof(ordinals) / sizeof(ordinals[0]); index++)
		if (ordinal == ordinals[index])
			return 1;
	return 0;
}

static int valid_external_section(
	const struct orlix_tcti_arm_xml_semantic_section *section)
{
	return section && section->locator && section->locator[0] &&
		section->section_count && valid_sha256(section->normalized_sha256) &&
		valid_sha256(section->shared_helpers_sha256);
}

static int emit_external_section(
	FILE *output, const struct orlix_tcti_arm_xml_semantic_section *section)
{
	return emit_string(output, section->locator) ||
		fputs(", ", output) == EOF ||
		emit_string(output, section->normalized_sha256) ||
		fprintf(output, ", %zuU, %zuU, ", section->section_count,
			section->shared_helper_count) < 0 ||
		emit_string(output, section->shared_helpers_sha256) ? -1 : 0;
}

static int valid_not_specified_operation(
	const char *source, size_t length,
	const struct orlix_tcti_target_operation *operation)
{
	const size_t body_length = sizeof(not_specified_body) - 1U;

	return operation &&
		operation->semantic_body_state ==
			ORLIX_TCTI_TARGET_OPERATION_BODY_PLACEHOLDER &&
		operation->semantic_body_source_length == body_length &&
		operation->semantic_body_source_offset <= length &&
		body_length <= length - operation->semantic_body_source_offset &&
		!memcmp(source + operation->semantic_body_source_offset,
			not_specified_body, body_length) &&
		valid_sha256(operation->semantic_body_sha256);
}

static int emit_ddi0602_row(
	FILE *output, size_t ordinal, const struct orlix_tcti_target_leaf *leaf,
	const struct orlix_tcti_arm_xml_semantic_entry *entry)
{
	if (fprintf(output, "ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(%zuU, ",
		    ordinal) < 0 ||
	    emit_string(output, leaf->name) || fputs(", ", output) == EOF ||
	    emit_string(output, entry->relative_file) ||
	    fputs(", ", output) == EOF ||
	    emit_external_section(output, &entry->decode) ||
	    fputs(", ", output) == EOF ||
	    emit_external_section(output, &entry->operation) ||
	    fputs(")\n", output) == EOF)
		return -1;
	return 0;
}

static int emit_not_specified_row(
	FILE *output, size_t ordinal, const struct orlix_tcti_target_leaf *leaf,
	const struct orlix_tcti_target_operation *operation)
{
	if (fprintf(output,
		    "ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(%zuU, ",
		    ordinal) < 0 ||
	    emit_string(output, leaf->name) ||
	    fputs(", \"Instructions.json#operations/", output) == EOF ||
	    fputs(operation->id, output) == EOF ||
	    fprintf(output, "/operation\", %zuU, %zuU, ",
		    operation->semantic_body_source_offset,
		    operation->semantic_body_source_length) < 0 ||
	    emit_string(output, operation->semantic_body_sha256) ||
	    fputs(")\n", output) == EOF)
		return -1;
	return 0;
}

enum orlix_tcti_target_semantic_provenance_error
orlix_tcti_target_semantic_provenance_emit(
	const char *source, size_t length,
	const struct orlix_tcti_arm_xml_package *package, FILE *output)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error import_error = { 0 };
	char source_digest[65];
	size_t ddi0602_count = 0;
	size_t not_specified_count = 0;
	size_t index;
	enum orlix_tcti_target_semantic_provenance_error result =
		ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_SOURCE;

	if (!source || !length || !package || !output ||
	    package->shared_ps_count != ORLIX_TCTI_ARM_XML_SHARED_PS_COUNT ||
	    package->shared_anchor_count != ORLIX_TCTI_ARM_XML_SHARED_ANCHOR_COUNT ||
	    package->index_form_count != ORLIX_TCTI_ARM_XML_INDEX_FORM_COUNT)
		return ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_INVALID_ARGUMENT;

	orlix_tcti_target_inventory_sha256(source, length, source_digest);
	if (orlix_tcti_target_inventory_import(source, length, &inventory,
					       &import_error) ||
	    inventory.leaf_count != ORLIX_TCTI_TARGET_LEAF_COUNT)
		return ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_SOURCE;

	for (index = 0; index < inventory.leaf_count; index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory.leaves[index];
		const struct orlix_tcti_target_operation *operation =
			semantic_operation(&inventory,
				orlix_tcti_target_inventory_operation(
					&inventory, leaf->operation_id));
		const struct orlix_tcti_arm_xml_semantic_entry *entry =
			orlix_tcti_arm_xml_package_entry(package, leaf->name);

		if (entry) {
			if (official_semantics_not_specified_ordinal(index) ||
			    !entry->relative_file || !entry->relative_file[0] ||
			    !valid_external_section(&entry->decode) ||
			    !valid_external_section(&entry->operation))
				goto out;
			ddi0602_count++;
			continue;
		}
		if (!official_semantics_not_specified_ordinal(index) ||
		    !valid_not_specified_operation(source, length, operation)) {
			result = ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_MISSING_OPERATION;
			goto out;
		}
		not_specified_count++;
	}

	if (ddi0602_count != ORLIX_TCTI_DDI0602_PROVENANCE_COUNT ||
	    not_specified_count != ORLIX_TCTI_NOT_SPECIFIED_PROVENANCE_COUNT)
		goto out;

	if (fprintf(output,
		    "ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(\""
		    "authority=Arm_DDI0602_2026_06|distribution=external_non_redistributed|"
		    "url=%s|archive_sha256=%s|release=%s|release_digest=%s|"
		    "shared_pseudocode_sha256=%s|notice_sha256=%s|index_sha256=%s|"
		    "aarchmrs=vFATAp1-A/build=818/ref=2026-06_rel|"
		    "instructions_sha256=%s|ddi0602_rows=%zu|"
		    "official_semantics_not_specified_rows=%zu\")\n",
		    ORLIX_TCTI_ARM_XML_SOURCE_URL,
		    ORLIX_TCTI_ARM_XML_ARCHIVE_SHA256,
		    ORLIX_TCTI_ARM_XML_RELEASE_NAME,
		    ORLIX_TCTI_ARM_XML_RELEASE_DIGEST,
		    ORLIX_TCTI_ARM_XML_SHARED_SHA256,
		    ORLIX_TCTI_ARM_XML_NOTICE_SHA256,
		    ORLIX_TCTI_ARM_XML_INDEX_SHA256,
		    source_digest, ddi0602_count, not_specified_count) < 0) {
		result = ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_IO;
		goto out;
	}

	for (index = 0; index < inventory.leaf_count; index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory.leaves[index];
		const struct orlix_tcti_target_operation *operation =
			semantic_operation(&inventory,
				orlix_tcti_target_inventory_operation(
					&inventory, leaf->operation_id));
		const struct orlix_tcti_arm_xml_semantic_entry *entry =
			orlix_tcti_arm_xml_package_entry(package, leaf->name);

		if ((entry && emit_ddi0602_row(output, index, leaf, entry)) ||
		    (!entry && emit_not_specified_row(output, index, leaf, operation))) {
			result = ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_IO;
			goto out;
		}
	}

	result = ferror(output) ? ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_IO :
		ORLIX_TCTI_TARGET_SEMANTIC_PROVENANCE_OK;
out:
	orlix_tcti_target_inventory_destroy(&inventory);
	return result;
}
