/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_asl_availability.h"

#include "target_inventory_import.h"

#include <stdio.h>
#include <string.h>

static int emit_string(FILE *output, const char *text)
{
	return fprintf(output, "\"%s\"", text) < 0 ? -1 : 0;
}

static const char *body_state_symbol(
	enum orlix_tcti_target_operation_body_state state)
{
	switch (state) {
	case ORLIX_TCTI_TARGET_OPERATION_BODY_ABSENT:
		return "ORLIX_TCTI_A64_ASL_BODY_ABSENT";
	case ORLIX_TCTI_TARGET_OPERATION_BODY_PLACEHOLDER:
		return "ORLIX_TCTI_A64_ASL_BODY_PLACEHOLDER";
	case ORLIX_TCTI_TARGET_OPERATION_BODY_PRESENT:
		return "ORLIX_TCTI_A64_ASL_BODY_PRESENT";
	}
	return NULL;
}

static const char *decode_state_symbol(
	enum orlix_tcti_target_operation_decode_state state)
{
	switch (state) {
	case ORLIX_TCTI_TARGET_OPERATION_DECODE_ABSENT:
		return "ORLIX_TCTI_A64_ASL_DECODE_ABSENT";
	case ORLIX_TCTI_TARGET_OPERATION_DECODE_NULL:
		return "ORLIX_TCTI_A64_ASL_DECODE_NULL";
	case ORLIX_TCTI_TARGET_OPERATION_DECODE_PRESENT:
		return "ORLIX_TCTI_A64_ASL_DECODE_PRESENT";
	}
	return NULL;
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

static const char *xml_state_symbol(
	const struct orlix_tcti_arm_xml_semantic_entry *entry)
{
	if (!entry)
		return "ORLIX_TCTI_A64_ASL_XML_ENCODING_ABSENT";
	if (!entry->decode.locator || !entry->operation.locator)
		return "ORLIX_TCTI_A64_ASL_XML_SEMANTICS_INCOMPLETE";
	return "ORLIX_TCTI_A64_ASL_XML_SEMANTICS_PRESENT";
}

static int emit_xml_section(
	FILE *output, const struct orlix_tcti_arm_xml_semantic_section *section)
{
	const char *locator = section && section->locator ? section->locator : "";
	const char *digest = section && section->locator ?
		section->normalized_sha256 : "";
	const char *helper_digest = section && section->locator ?
		section->shared_helpers_sha256 : "";
	size_t section_count = section ? section->section_count : 0U;
	size_t helper_count = section ? section->shared_helper_count : 0U;

	return fprintf(output, "%zuU, ", section_count) < 0 ||
		emit_string(output, locator) || fputs(", ", output) == EOF ||
		emit_string(output, digest) ||
		fprintf(output, ", %zuU, ", helper_count) < 0 ||
		emit_string(output, helper_digest) ? -1 : 0;
}

enum orlix_tcti_target_asl_availability_error
orlix_tcti_target_asl_availability_emit(
	const char *source, size_t length,
	const struct orlix_tcti_arm_xml_package *package, FILE *output)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error import_error = { 0 };
	size_t concrete_operations = 0;
	size_t operation_aliases = 0;
	size_t xml_matched = 0;
	size_t xml_complete = 0;
	size_t xml_decode_missing = 0;
	size_t xml_operation_missing = 0;
	size_t index;
	char source_digest[65];
	enum orlix_tcti_target_asl_availability_error result =
		ORLIX_TCTI_TARGET_ASL_AVAILABILITY_SOURCE;

	if (!source || !length || !package || !output ||
	    package->shared_ps_count != ORLIX_TCTI_ARM_XML_SHARED_PS_COUNT ||
	    package->shared_anchor_count != ORLIX_TCTI_ARM_XML_SHARED_ANCHOR_COUNT ||
	    package->index_form_count != ORLIX_TCTI_ARM_XML_INDEX_FORM_COUNT ||
	    package->entry_count != ORLIX_TCTI_ARM_XML_ENCODING_COUNT)
		return ORLIX_TCTI_TARGET_ASL_AVAILABILITY_INVALID_ARGUMENT;
	if (orlix_tcti_target_inventory_import(source, length, &inventory,
					       &import_error))
		goto out;
	orlix_tcti_target_inventory_sha256(source, length, source_digest);
	for (index = 0; index < inventory.operation_count; index++) {
		const struct orlix_tcti_target_operation *operation =
			&inventory.operations[index];

		if (operation->is_alias) {
			operation_aliases++;
			continue;
		}
		concrete_operations++;
		if (operation->semantic_body_state !=
				ORLIX_TCTI_TARGET_OPERATION_BODY_PLACEHOLDER ||
		    operation->decode_state !=
				ORLIX_TCTI_TARGET_OPERATION_DECODE_NULL)
			goto out;
	}
	if (inventory.operation_count != 2871U || concrete_operations != 2656U ||
	    operation_aliases != 215U)
		goto out;
	for (index = 0; index < inventory.leaf_count; index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory.leaves[index];
		const struct orlix_tcti_target_operation *semantic =
			semantic_operation(&inventory,
				orlix_tcti_target_inventory_operation(
					&inventory, leaf->operation_id));
		const struct orlix_tcti_arm_xml_semantic_entry *entry =
			orlix_tcti_arm_xml_package_entry(package, leaf->name);

		if (!semantic || semantic->semantic_body_state !=
				ORLIX_TCTI_TARGET_OPERATION_BODY_PLACEHOLDER ||
		    semantic->decode_state !=
				ORLIX_TCTI_TARGET_OPERATION_DECODE_NULL) {
			result = ORLIX_TCTI_TARGET_ASL_AVAILABILITY_MISSING_OPERATION;
			goto out;
		}
		if (!entry)
			continue;
		xml_matched++;
		if (!entry->decode.locator)
			xml_decode_missing++;
		if (!entry->operation.locator)
			xml_operation_missing++;
		if (entry->decode.locator && entry->operation.locator)
			xml_complete++;
	}
	if (fprintf(output,
		"ORLIX_TCTI_A64_ASL_AVAILABILITY_SOURCE(\""
		"arm_a64_isa_xml_a_profile_2026_06|url=%s|archive_sha256=%s|"
		"release_path=%s|release_digest=%s|release_files=%u|xml_files=%u|"
		"encodings=%u|instruction_semantic_sections=%u|"
		"shared_pseudocode.xml=%s|notice.xml=%s|index.xml=%s|"
		"license=%s|"
		"aarchmrs=vFATAp1-A/build=818/ref=2026-06_rel|instructions_sha256=%s|"
		"compatibility=both_arm_2026_06_a_profile|shared_ps=%u|"
		"shared_anchors=%u|index_forms=%u|aarchmrs_operations=2871|"
		"concrete_operations=2656|operation_aliases=215|"
		"instruction_bodies=2656_placeholder|decodes=2656_null|"
		"target_xml_matched=%zu|target_xml_encoding_absent=%zu|"
		"target_xml_semantics_complete=%zu|target_xml_decode_missing=%zu|"
		"target_xml_operation_missing=%zu\", "
		"ORLIX_TCTI_A64_ASL_CORPUS_PRESENT, "
		"ORLIX_TCTI_A64_ASL_HELPERS_AVAILABLE)\n",
		ORLIX_TCTI_ARM_XML_SOURCE_URL, ORLIX_TCTI_ARM_XML_ARCHIVE_SHA256,
		ORLIX_TCTI_ARM_XML_RELEASE_NAME, ORLIX_TCTI_ARM_XML_RELEASE_DIGEST,
		ORLIX_TCTI_ARM_XML_RELEASE_FILE_COUNT,
		ORLIX_TCTI_ARM_XML_INSTRUCTION_FILE_COUNT,
		ORLIX_TCTI_ARM_XML_ENCODING_COUNT,
		ORLIX_TCTI_ARM_XML_INSTRUCTION_PS_COUNT,
		ORLIX_TCTI_ARM_XML_SHARED_SHA256, ORLIX_TCTI_ARM_XML_NOTICE_SHA256,
		ORLIX_TCTI_ARM_XML_INDEX_SHA256, ORLIX_TCTI_ARM_XML_LICENSE_CLASS,
		source_digest,
		ORLIX_TCTI_ARM_XML_SHARED_PS_COUNT,
		ORLIX_TCTI_ARM_XML_SHARED_ANCHOR_COUNT,
		ORLIX_TCTI_ARM_XML_INDEX_FORM_COUNT, xml_matched,
		inventory.leaf_count - xml_matched, xml_complete,
		xml_decode_missing, xml_operation_missing) < 0) {
		result = ORLIX_TCTI_TARGET_ASL_AVAILABILITY_IO;
		goto out;
	}
	for (index = 0; index < inventory.leaf_count; index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory.leaves[index];
		const struct orlix_tcti_target_operation *semantic =
			semantic_operation(&inventory,
				orlix_tcti_target_inventory_operation(
					&inventory, leaf->operation_id));
		const struct orlix_tcti_arm_xml_semantic_entry *entry =
			orlix_tcti_arm_xml_package_entry(package, leaf->name);
		const char *body_state = body_state_symbol(semantic->semantic_body_state);
		const char *decode_state = decode_state_symbol(semantic->decode_state);

		if (!body_state || !decode_state ||
		    fputs("ORLIX_TCTI_A64_ASL_AVAILABILITY_ROW(", output) == EOF ||
		    fprintf(output, "%zuU, ", index) < 0 ||
		    emit_string(output, leaf->name) || fputs(", ", output) == EOF ||
		    emit_string(output, leaf->operation_id) ||
		    fputs(", ", output) == EOF || emit_string(output, semantic->id) ||
		    fprintf(output,
			", \"operations/%s/operation\", %zuU, %zuU, %zuU, %zuU, ",
			semantic->id, semantic->semantic_member_source_offset,
			semantic->semantic_member_source_length,
			semantic->semantic_body_source_offset,
			semantic->semantic_body_source_length) < 0 ||
		    emit_string(output, semantic->semantic_body_sha256) ||
		    fprintf(output,
			", %s, \"operations/%s/decode\", %zuU, %zuU, %zuU, %zuU, ",
			body_state, semantic->id, semantic->decode_member_source_offset,
			semantic->decode_member_source_length,
			semantic->decode_source_offset,
			semantic->decode_source_length) < 0 ||
		    emit_string(output, semantic->decode_sha256) ||
		    fprintf(output, ", %s, ORLIX_TCTI_A64_ASL_CORPUS_PRESENT, "
			"ORLIX_TCTI_A64_ASL_HELPERS_AVAILABLE)\n", decode_state) < 0 ||
		    fputs("ORLIX_TCTI_A64_ASL_XML_ROW(", output) == EOF ||
		    fprintf(output, "%zuU, ", index) < 0 ||
		    emit_string(output, leaf->name) ||
		    fprintf(output, ", %s, ", xml_state_symbol(entry)) < 0 ||
		    emit_string(output, entry ? entry->relative_file : "") ||
		    fputs(", ", output) == EOF ||
		    emit_xml_section(output, entry ? &entry->decode : NULL) ||
		    fputs(", ", output) == EOF ||
		    emit_xml_section(output, entry ? &entry->operation : NULL) ||
		    fputs(")\n", output) == EOF) {
			result = ORLIX_TCTI_TARGET_ASL_AVAILABILITY_IO;
			goto out;
		}
	}
	result = ferror(output) ? ORLIX_TCTI_TARGET_ASL_AVAILABILITY_IO :
		ORLIX_TCTI_TARGET_ASL_AVAILABILITY_OK;
out:
	orlix_tcti_target_inventory_destroy(&inventory);
	return result;
}
