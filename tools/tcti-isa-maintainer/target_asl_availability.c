/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_asl_availability.h"
#include "target_inventory_import.h"

#include <inttypes.h>
#include <stdio.h>

static int emit_string(FILE *output, const char *text)
{
	return fprintf(output, "\"%s\"", text) < 0 ? -1 : 0;
}

enum tcti_target_asl_availability_error
tcti_target_asl_availability_emit(const char *source, size_t length,
				  FILE *output)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error import_error = { 0 };
	size_t index;
	enum tcti_target_asl_availability_error result =
		TCTI_TARGET_ASL_AVAILABILITY_SOURCE;

	if (!source || !output)
		return TCTI_TARGET_ASL_AVAILABILITY_INVALID_ARGUMENT;
	if (tcti_target_inventory_import(source, length, &inventory, &import_error))
		return TCTI_TARGET_ASL_AVAILABILITY_SOURCE;
	/* The source is authoritative but supplies no separate shared-ASL corpus. */
	if (fputs("TCTI_A64_ASL_AVAILABILITY_SOURCE(\"inline_aarchmrs_operations_v2\", \"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\", \"shared_asl_absent_blocking\")\n", output) == EOF)
		goto out;
	for (index = 0; index < inventory.leaf_count; index++) {
		const struct tcti_target_leaf *leaf = &inventory.leaves[index];
		const struct tcti_target_operation *operation =
			tcti_target_inventory_operation(&inventory, leaf->operation_id);
		char note_digest[65] = { 0 };
		const char *note_presence;

		if (!operation) {
			result = TCTI_TARGET_ASL_AVAILABILITY_MISSING_OPERATION;
			goto out;
		}
		note_presence = operation->operational_note_present ? "present" :
			"absent";
		if (operation->operational_note_present)
			tcti_target_inventory_sha256(
				source + operation->operational_note_source_offset,
				operation->operational_note_source_length, note_digest);
		if (fputs("TCTI_A64_ASL_AVAILABILITY_ROW(", output) == EOF ||
		    fprintf(output, "%zuU, ", index) < 0 ||
		    emit_string(output, leaf->name) || fputs(", ", output) == EOF ||
		    emit_string(output, leaf->operation_id) ||
		    fprintf(output, ", \"operations/%s\", %zuU, %zuU, ",
			    operation->id, operation->source_offset, operation->source_length) < 0 ||
		    emit_string(output, note_presence) ||
		    fprintf(output, ", %zuU, %zuU, ",
			    operation->operational_note_source_offset,
			    operation->operational_note_source_length) < 0 ||
		    emit_string(output, note_digest) ||
		    fputs(", \"shared_asl_absent_blocking\")\n", output) == EOF) {
			result = TCTI_TARGET_ASL_AVAILABILITY_IO;
			goto out;
		}
	}
	result = ferror(output) ? TCTI_TARGET_ASL_AVAILABILITY_IO :
		TCTI_TARGET_ASL_AVAILABILITY_OK;
out:
	tcti_target_inventory_destroy(&inventory);
	return result;
}
