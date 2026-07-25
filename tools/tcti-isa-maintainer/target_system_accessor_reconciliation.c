// SPDX-License-Identifier: GPL-2.0-only
#include "target_system_accessor_reconciliation.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct tcti_system_accessor_form_mapping {
	const char *name;
	enum tcti_system_accessor_generic_leaf leaf;
	enum tcti_system_accessor_direction direction;
};

/*
 * This table is deliberately exhaustive for the pinned A64 accessor names.
 * New Arm names remain unsupported until they receive an explicit source-leaf
 * relationship.  There is no prefix or opcode-family fallback.
 */
static const struct tcti_system_accessor_form_mapping form_mappings[] = {
	{ "A64.APAS", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.AT", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.BRB", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.CFP", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.COSP", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.CPP", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.DC", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.DVP", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPOPCX", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPOPM", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPOPX", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPUSHM", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPUSHX", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSSS1", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSSS2", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GIC", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GICR", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GSB", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.IC", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.MRS", TCTI_SYSTEM_ACCESSOR_LEAF_MRS_RS_SYSTEMMOVE,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_READ },
	{ "A64.MRRS", TCTI_SYSTEM_ACCESSOR_LEAF_MRRS_RS_SYSTEMMOVEPR,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_READ },
	{ "A64.MSRimmediate", TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SI_PSTATE,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE },
	{ "A64.MSRregister", TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SR_SYSTEMMOVE,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE },
	{ "A64.MSRRregister", TCTI_SYSTEM_ACCESSOR_LEAF_MSRR_SR_SYSTEMMOVEPR,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE },
	{ "A64.PLBI", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.SYS", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.SYSL", TCTI_SYSTEM_ACCESSOR_LEAF_SYSL_RC_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_READ },
	{ "A64.SYSP", TCTI_SYSTEM_ACCESSOR_LEAF_SYSP_CR_SYSPAIRINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.TLBI", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.TLBIP", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.TRCIT", TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
};

static int is_system_accessor(const struct tcti_register_accessor *accessor)
{
	return accessor && accessor->type &&
		(!strcmp(accessor->type, "Accessors.SystemAccessor") ||
		 !strcmp(accessor->type, "Accessors.SystemAccessorArray"));
}

static int is_aarch64_system_accessor(const struct tcti_register_accessor *accessor)
{
	return is_system_accessor(accessor) && accessor->name &&
		!strncmp(accessor->name, "A64.", 4U);
}

static const struct tcti_system_accessor_form_mapping *find_mapping(const char *name)
{
	size_t index;

	if (!name)
		return NULL;
	for (index = 0; index < sizeof(form_mappings) / sizeof(form_mappings[0]);
	     index++)
		if (!strcmp(form_mappings[index].name, name))
			return &form_mappings[index];
	return NULL;
}

static uint32_t selector_count_for_encoding(const struct tcti_register_model *model,
					    uint32_t encoding_index)
{
	size_t index;
	uint32_t count = 0;

	for (index = 0; index < model->system_selector_count; index++)
		count += model->system_selectors[index].encoding_index == encoding_index;
	return count;
}

static void count_disposition(struct tcti_system_accessor_reconciliation_result *result,
				      enum tcti_system_accessor_disposition disposition)
{
	switch (disposition) {
	case TCTI_SYSTEM_ACCESSOR_MAPPED:
		result->census.mapped++;
		break;
	case TCTI_SYSTEM_ACCESSOR_UNSUPPORTED:
		result->census.unsupported++;
		break;
	case TCTI_SYSTEM_ACCESSOR_AMBIGUOUS:
		result->census.ambiguous++;
		break;
	case TCTI_SYSTEM_ACCESSOR_INVALID:
		result->census.invalid++;
		break;
	}
}

enum tcti_system_accessor_reconciliation_error
tcti_system_accessor_reconcile(
	const struct tcti_register_model *model,
	struct tcti_system_accessor_reconciliation_result *result)
{
	size_t accessor_index;
	size_t count = 0;
	size_t output_index = 0;

	if (!model || !result || (!model->accessors && model->accessor_count) ||
	    (!model->system_encodings && model->system_encoding_count) ||
	    (!model->system_selectors && model->system_selector_count))
		return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT;
	memset(result, 0, sizeof(*result));
	for (accessor_index = 0; accessor_index < model->accessor_count;
	     accessor_index++)
		count += is_aarch64_system_accessor(&model->accessors[accessor_index]);
	if (count > SIZE_MAX / sizeof(*result->entries))
		return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY;
	if (count) {
		result->entries = calloc(count, sizeof(*result->entries));
		if (!result->entries)
			return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY;
	}
	result->entry_count = count;
	result->census.aarch64_accessors = count;

	for (accessor_index = 0; accessor_index < model->accessor_count;
	     accessor_index++) {
		const struct tcti_register_accessor *accessor =
			&model->accessors[accessor_index];
		const struct tcti_system_accessor_form_mapping *mapping;
		struct tcti_system_accessor_reconciliation_entry *entry;

		if (!is_aarch64_system_accessor(accessor))
			continue;
		entry = &result->entries[output_index++];
		entry->accessor_index = (uint32_t)accessor_index;
		entry->encoding_index = UINT32_MAX;
		entry->condition_expression = accessor->condition_expression;
		entry->accessor_name = accessor->name;
		entry->accessor_source_offset = accessor->source_offset;
		entry->accessor_source_length = accessor->source_length;

		if (accessor->first_system_encoding == UINT32_MAX ||
		    accessor->system_encoding_count != 1U ||
		    accessor->first_system_encoding >= model->system_encoding_count ||
		    accessor->system_encoding_count >
			model->system_encoding_count - accessor->first_system_encoding) {
			entry->disposition = accessor->system_encoding_count > 1U ?
				TCTI_SYSTEM_ACCESSOR_AMBIGUOUS :
				TCTI_SYSTEM_ACCESSOR_INVALID;
			count_disposition(result, entry->disposition);
			continue;
		}
		entry->encoding_index = accessor->first_system_encoding;
		entry->encoding_source_offset =
			model->system_encodings[entry->encoding_index].source_offset;
		entry->encoding_source_length =
			model->system_encodings[entry->encoding_index].source_length;
		entry->selector_count = selector_count_for_encoding(model,
			entry->encoding_index);
		mapping = find_mapping(accessor->name);
		if (!mapping) {
			entry->disposition = TCTI_SYSTEM_ACCESSOR_UNSUPPORTED;
			count_disposition(result, entry->disposition);
			continue;
		}
		if (!entry->selector_count) {
			entry->disposition = TCTI_SYSTEM_ACCESSOR_INVALID;
			count_disposition(result, entry->disposition);
			continue;
		}
		entry->generic_leaf = mapping->leaf;
		entry->direction = mapping->direction;
		entry->disposition = TCTI_SYSTEM_ACCESSOR_MAPPED;
		count_disposition(result, entry->disposition);
	}
	return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK;
}

enum tcti_system_accessor_reconciliation_error
tcti_system_accessor_reconciliation_validate(
	const struct tcti_system_accessor_reconciliation_result *result)
{
	if (!result || (!result->entries && result->entry_count) ||
	    result->census.aarch64_accessors != result->entry_count ||
	    result->census.mapped + result->census.unsupported +
		result->census.ambiguous + result->census.invalid != result->entry_count)
		return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT;
	if (result->census.ambiguous)
		return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_AMBIGUOUS;
	if (result->census.invalid)
		return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID;
	if (result->census.unsupported)
		return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_UNSUPPORTED;
	return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK;
}

static int emit_c_string(FILE *output, const char *string)
{
	const unsigned char *cursor = (const unsigned char *)string;

	if (fputc('"', output) == EOF)
		return -1;
	while (cursor && *cursor) {
		if (*cursor == '"' || *cursor == '\\') {
			if (fputc('\\', output) == EOF || fputc(*cursor, output) == EOF)
				return -1;
		} else if (*cursor >= 0x20U && *cursor <= 0x7eU) {
			if (fputc(*cursor, output) == EOF)
				return -1;
		} else if (fprintf(output, "\\%03o", *cursor) < 0) {
			return -1;
		}
		cursor++;
	}
	return fputc('"', output) == EOF ? -1 : 0;
}

enum tcti_system_accessor_reconciliation_error
tcti_system_accessor_reconciliation_emit(
	const struct tcti_register_model *model, FILE *output)
{
	struct tcti_system_accessor_reconciliation_result result = { 0 };
	enum tcti_system_accessor_reconciliation_error error;
	size_t index;

	if (!output)
		return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT;
	error = tcti_system_accessor_reconcile(model, &result);
	if (error)
		return error;
	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated from pinned Arm Registers.json. Do not edit. */\n"
		  "TCTI_A64_SYSTEM_ACCESSOR_SOURCE(\"vFATAp1-A\", \"818\", "
		  "\"2026-06_rel\", \"2.9.5\", \"2026-06-24 17:12:14\", "
		  "\"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874\")\n",
		  output) == EOF ||
	    fprintf(output, "TCTI_A64_SYSTEM_ACCESSOR_COUNTS(%zuU, %zuU, %zuU, "
		    "%zuU, %zuU)\n", result.census.aarch64_accessors,
		    result.census.mapped, result.census.unsupported,
		    result.census.ambiguous, result.census.invalid) < 0)
		goto io;
	for (index = 0; index < result.entry_count; index++) {
		const struct tcti_system_accessor_reconciliation_entry *entry =
			&result.entries[index];

		if (fprintf(output, "TCTI_A64_SYSTEM_ACCESSOR(%" PRIu32 "U, "
		    "%" PRIu32 "U, ", entry->accessor_index,
		    entry->encoding_index) < 0 ||
		    emit_c_string(output, entry->accessor_name) ||
		    fprintf(output, ", ") < 0 ||
		    emit_c_string(output,
			 tcti_system_accessor_generic_leaf_name(entry->generic_leaf)) ||
		    fprintf(output, ", %uU, %uU, %uU, %" PRIu32 "U, "
		    "%zuU, %zuU, %zuU, %zuU)\n",
		    (unsigned int)entry->direction,
		    (unsigned int)entry->disposition, entry->selector_count,
		    entry->condition_expression, entry->accessor_source_offset,
		    entry->accessor_source_length, entry->encoding_source_offset,
		    entry->encoding_source_length) < 0)
			goto io;
	}
	tcti_system_accessor_reconciliation_destroy(&result);
	return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK;
io:
	tcti_system_accessor_reconciliation_destroy(&result);
	return TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID;
}

void tcti_system_accessor_reconciliation_destroy(
	struct tcti_system_accessor_reconciliation_result *result)
{
	if (!result)
		return;
	free(result->entries);
	memset(result, 0, sizeof(*result));
}

const char *tcti_system_accessor_generic_leaf_name(
	enum tcti_system_accessor_generic_leaf leaf)
{
	switch (leaf) {
	case TCTI_SYSTEM_ACCESSOR_LEAF_MRS_RS_SYSTEMMOVE:
		return "MRS_RS_systemmove";
	case TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SI_PSTATE:
		return "MSR_SI_pstate";
	case TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SR_SYSTEMMOVE:
		return "MSR_SR_systemmove";
	case TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS:
		return "SYS_CR_systeminstrs";
	case TCTI_SYSTEM_ACCESSOR_LEAF_SYSL_RC_SYSTEMINSTRS:
		return "SYSL_RC_systeminstrs";
	case TCTI_SYSTEM_ACCESSOR_LEAF_SYSP_CR_SYSPAIRINSTRS:
		return "SYSP_CR_syspairinstrs";
	case TCTI_SYSTEM_ACCESSOR_LEAF_MSRR_SR_SYSTEMMOVEPR:
		return "MSRR_SR_systemmovepr";
	case TCTI_SYSTEM_ACCESSOR_LEAF_MRRS_RS_SYSTEMMOVEPR:
		return "MRRS_RS_systemmovepr";
	case TCTI_SYSTEM_ACCESSOR_LEAF_NONE:
	default:
		return "";
	}
}

const char *tcti_system_accessor_reconciliation_error_name(
	enum tcti_system_accessor_reconciliation_error error)
{
	switch (error) {
	case TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK:
		return "ok";
	case TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT:
		return "invalid argument";
	case TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY:
		return "out of memory";
	case TCTI_SYSTEM_ACCESSOR_RECONCILIATION_UNSUPPORTED:
		return "unsupported accessor form";
	case TCTI_SYSTEM_ACCESSOR_RECONCILIATION_AMBIGUOUS:
		return "ambiguous accessor encoding";
	case TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID:
		return "invalid accessor encoding";
	default:
		return "unknown error";
	}
}
