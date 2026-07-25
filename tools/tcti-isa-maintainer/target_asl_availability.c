/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_asl_availability.h"
#include "target_inventory_import.h"

#include <inttypes.h>
#include <stdio.h>

static int emit_string(FILE *output, const char *text)
{
	return fprintf(output, "\"%s\"", text) < 0 ? -1 : 0;
}

static const char *body_state_symbol(
	enum tcti_target_operation_body_state state)
{
	switch (state) {
	case TCTI_TARGET_OPERATION_BODY_ABSENT:
		return "TCTI_A64_ASL_BODY_ABSENT";
	case TCTI_TARGET_OPERATION_BODY_PLACEHOLDER:
		return "TCTI_A64_ASL_BODY_PLACEHOLDER";
	case TCTI_TARGET_OPERATION_BODY_PRESENT:
		return "TCTI_A64_ASL_BODY_PRESENT";
	}
	return NULL;
}

static const char *decode_state_symbol(
	enum tcti_target_operation_decode_state state)
{
	switch (state) {
	case TCTI_TARGET_OPERATION_DECODE_ABSENT:
		return "TCTI_A64_ASL_DECODE_ABSENT";
	case TCTI_TARGET_OPERATION_DECODE_NULL:
		return "TCTI_A64_ASL_DECODE_NULL";
	case TCTI_TARGET_OPERATION_DECODE_PRESENT:
		return "TCTI_A64_ASL_DECODE_PRESENT";
	}
	return NULL;
}

static const struct tcti_target_operation *semantic_operation(
	const struct tcti_target_inventory *inventory,
	const struct tcti_target_operation *operation)
{
	if (!operation)
		return NULL;
	if (!operation->is_alias)
		return operation;
	if (!operation->canonical_operation_id)
		return NULL;
	return tcti_target_inventory_operation(inventory,
				       operation->canonical_operation_id);
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
	/* The source is authoritative but supplies no shared-ASL corpus or helpers. */
	if (fputs("TCTI_A64_ASL_AVAILABILITY_SOURCE(\"inline_aarchmrs_operations_v3\", \"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\", TCTI_A64_ASL_CORPUS_ABSENT, TCTI_A64_ASL_HELPERS_UNAVAILABLE)\n", output) == EOF)
		goto out;
	for (index = 0; index < inventory.leaf_count; index++) {
		const struct tcti_target_leaf *leaf = &inventory.leaves[index];
		const struct tcti_target_operation *operation =
			tcti_target_inventory_operation(&inventory, leaf->operation_id);
		const struct tcti_target_operation *semantic;
		const char *body_state;
		const char *decode_state;

		if (!operation) {
			result = TCTI_TARGET_ASL_AVAILABILITY_MISSING_OPERATION;
			goto out;
		}
		semantic = semantic_operation(&inventory, operation);
		body_state = semantic ? body_state_symbol(semantic->semantic_body_state) :
			NULL;
		decode_state = semantic ? decode_state_symbol(semantic->decode_state) :
			NULL;
		if (!semantic || !body_state || !decode_state) {
			result = TCTI_TARGET_ASL_AVAILABILITY_MISSING_OPERATION;
			goto out;
		}
		if (fputs("TCTI_A64_ASL_AVAILABILITY_ROW(", output) == EOF ||
		    fprintf(output, "%zuU, ", index) < 0 ||
		    emit_string(output, leaf->name) || fputs(", ", output) == EOF ||
		    emit_string(output, leaf->operation_id) || fputs(", ", output) == EOF ||
		    emit_string(output, semantic->id) ||
		    fprintf(output, ", \"operations/%s/operation\", %zuU, %zuU, "
			    "%zuU, %zuU, ", semantic->id,
			    semantic->semantic_member_source_offset,
			    semantic->semantic_member_source_length,
			    semantic->semantic_body_source_offset,
			    semantic->semantic_body_source_length) < 0 ||
		    emit_string(output, semantic->semantic_body_sha256) ||
		    fprintf(output, ", %s, \"operations/%s/decode\", %zuU, %zuU, "
			    "%zuU, %zuU, ", body_state, semantic->id,
			    semantic->decode_member_source_offset,
			    semantic->decode_member_source_length,
			    semantic->decode_source_offset,
			    semantic->decode_source_length) < 0 ||
		    emit_string(output, semantic->decode_sha256) ||
		    fprintf(output, ", %s, TCTI_A64_ASL_CORPUS_ABSENT, "
			    "TCTI_A64_ASL_HELPERS_UNAVAILABLE)\n", decode_state) < 0) {
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
