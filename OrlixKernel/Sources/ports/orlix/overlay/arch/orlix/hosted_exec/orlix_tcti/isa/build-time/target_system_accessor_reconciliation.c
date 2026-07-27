// SPDX-License-Identifier: GPL-2.0-only
#include "target_system_accessor_reconciliation.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct orlix_tcti_system_accessor_form_mapping {
	const char *name;
	enum orlix_tcti_system_accessor_generic_leaf leaf;
	enum orlix_tcti_system_accessor_direction direction;
};

/*
 * This table is deliberately exhaustive for the pinned A64 accessor names.
 * New Arm names remain unsupported until they receive an explicit source-leaf
 * relationship.  There is no prefix or opcode-family fallback.
 */
static const struct orlix_tcti_system_accessor_form_mapping form_mappings[] = {
	{ "A64.APAS", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.AT", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.BRB", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.CFP", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.COSP", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.CPP", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.DC", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.DVP", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPOPCX", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPOPM", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPOPX", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPUSHM", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSPUSHX", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSSS1", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GCSSS2", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GIC", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GICR", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.GSB", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.IC", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.MRS", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MRS_RS_SYSTEMMOVE,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_READ },
	{ "A64.MRRS", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MRRS_RS_SYSTEMMOVEPR,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_READ },
	{ "A64.MSRimmediate", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SI_PSTATE,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE },
	{ "A64.MSRregister", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SR_SYSTEMMOVE,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE },
	{ "A64.MSRRregister", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSRR_SR_SYSTEMMOVEPR,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE },
	{ "A64.PLBI", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.SYS", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.SYSL", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYSL_RC_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_READ },
	{ "A64.SYSP", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYSP_CR_SYSPAIRINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.TLBI", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.TLBIP", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
	{ "A64.TRCIT", ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	  ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE },
};

static int is_system_accessor(const struct orlix_tcti_register_accessor *accessor)
{
	return accessor && accessor->type &&
		(!strcmp(accessor->type, "Accessors.SystemAccessor") ||
		 !strcmp(accessor->type, "Accessors.SystemAccessorArray"));
}

static int is_aarch64_system_accessor(const struct orlix_tcti_register_accessor *accessor)
{
	return is_system_accessor(accessor) && accessor->name &&
		!strncmp(accessor->name, "A64.", 4U);
}

static const struct orlix_tcti_system_accessor_form_mapping *find_mapping(const char *name)
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

static uint32_t selector_count_for_encoding(const struct orlix_tcti_register_model *model,
					    uint32_t encoding_index)
{
	size_t index;
	uint32_t count = 0;

	for (index = 0; index < model->system_selector_count; index++)
		count += model->system_selectors[index].encoding_index == encoding_index;
	return count;
}

/* FNV-1a fingerprints exact source bytes with unambiguous scalar boundaries. */
static uint64_t identity_byte(uint64_t identity, unsigned char byte)
{
	return (identity ^ byte) * UINT64_C(1099511628211);
}

static uint64_t identity_u64(uint64_t identity, uint64_t value)
{
	unsigned int index;

	for (index = 0; index < 8U; index++)
		identity = identity_byte(identity,
			(unsigned char)(value >> (index * 8U)));
	return identity;
}

static uint64_t identity_bytes(uint64_t identity, const char *bytes,
				       size_t length)
{
	size_t index;

	identity = identity_u64(identity, length);
	for (index = 0; index < length; index++)
		identity = identity_byte(identity, (unsigned char)bytes[index]);
	return identity;
}

static int model_source_span_valid(const struct orlix_tcti_register_model *model,
				  size_t offset, size_t length)
{
	return model && model->source && length && offset < model->source_length &&
		length <= model->source_length - offset;
}

static int selector_identity_for_encoding(const struct orlix_tcti_register_model *model,
					 uint32_t encoding_index,
					 uint64_t *identity_out)
{
	uint64_t identity = UINT64_C(1469598103934665603);
	size_t index;
	uint32_t count = 0;

	if (!model || !identity_out)
		return -1;
	identity = identity_u64(identity, encoding_index);
	for (index = 0; index < model->system_selector_count; index++) {
		const struct orlix_tcti_register_system_selector *selector =
			&model->system_selectors[index];

		if (selector->encoding_index != encoding_index)
			continue;
		if (!model_source_span_valid(model, selector->source_offset,
					     selector->source_length))
			return -1;
		identity = identity_u64(identity, index);
		identity = identity_bytes(identity,
			model->source + selector->source_offset,
			selector->source_length);
		count++;
	}
	if (!count)
		return -1;
	*identity_out = identity;
	return 0;
}

static int condition_identity_for_accessor(const struct orlix_tcti_register_model *model,
					  const struct orlix_tcti_register_accessor *accessor,
					  uint64_t *identity_out)
{
	uint64_t identity = UINT64_C(1469598103934665603);

	if (!model || !accessor || !identity_out)
		return -1;
	identity = identity_u64(identity, accessor->condition_expression);
	if (accessor->condition_expression == UINT32_MAX) {
		if (accessor->condition_offset || accessor->condition_length)
			return -1;
		*identity_out = identity;
		return 0;
	}
	if (accessor->condition_expression >= model->expression_count ||
	    !model_source_span_valid(model, accessor->condition_offset,
				     accessor->condition_length))
		return -1;
	*identity_out = identity_bytes(identity,
		model->source + accessor->condition_offset,
		accessor->condition_length);
	return 0;
}

static uint64_t reconciliation_identity(
	const struct orlix_tcti_system_accessor_reconciliation_result *result)
{
	uint64_t identity = UINT64_C(1469598103934665603);
	size_t index;

	if (!result)
		return 0;
	identity = identity_u64(identity, result->entry_count);
	for (index = 0; index < result->entry_count; index++) {
		const struct orlix_tcti_system_accessor_reconciliation_entry *entry =
			&result->entries[index];

		identity = identity_u64(identity, entry->accessor_index);
		identity = identity_u64(identity, entry->encoding_index);
		identity = identity_bytes(identity, entry->accessor_name,
			strlen(entry->accessor_name));
		identity = identity_bytes(identity,
			orlix_tcti_system_accessor_generic_leaf_name(entry->generic_leaf),
			strlen(orlix_tcti_system_accessor_generic_leaf_name(entry->generic_leaf)));
		identity = identity_u64(identity, entry->direction);
		identity = identity_u64(identity, entry->disposition);
		identity = identity_u64(identity, entry->selector_count);
		identity = identity_u64(identity, entry->condition_expression);
		identity = identity_u64(identity, entry->selector_identity);
		identity = identity_u64(identity, entry->condition_identity);
		identity = identity_u64(identity, entry->accessor_source_offset);
		identity = identity_u64(identity, entry->accessor_source_length);
		identity = identity_u64(identity, entry->encoding_source_offset);
		identity = identity_u64(identity, entry->encoding_source_length);
		identity = identity_u64(identity, entry->condition_source_offset);
		identity = identity_u64(identity, entry->condition_source_length);
	}
	return identity;
}

static void count_disposition(struct orlix_tcti_system_accessor_reconciliation_result *result,
				      enum orlix_tcti_system_accessor_disposition disposition)
{
	switch (disposition) {
	case ORLIX_TCTI_SYSTEM_ACCESSOR_MAPPED:
		result->census.mapped++;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RESERVED:
		result->census.reserved++;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_PRIVILEGED:
		result->census.privileged++;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_UNSUPPORTED:
		result->census.unsupported++;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_AMBIGUOUS:
		result->census.ambiguous++;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_CONTRADICTORY:
		result->census.contradictory++;
		break;
	case ORLIX_TCTI_SYSTEM_ACCESSOR_INVALID:
		result->census.invalid++;
		break;
	}
}

enum orlix_tcti_system_accessor_reconciliation_error
orlix_tcti_system_accessor_reconcile(
	const struct orlix_tcti_register_model *model,
	struct orlix_tcti_system_accessor_reconciliation_result *result)
{
	size_t accessor_index;
	size_t count = 0;
	size_t output_index = 0;

	if (!model || !result || (!model->accessors && model->accessor_count) ||
	    (!model->system_encodings && model->system_encoding_count) ||
	    (!model->system_selectors && model->system_selector_count))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT;
	memset(result, 0, sizeof(*result));
	for (accessor_index = 0; accessor_index < model->accessor_count;
	     accessor_index++)
		count += is_aarch64_system_accessor(&model->accessors[accessor_index]);
	if (count > SIZE_MAX / sizeof(*result->entries))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY;
	if (count) {
		result->entries = calloc(count, sizeof(*result->entries));
		if (!result->entries)
			return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY;
	}
	result->entry_count = count;
	result->census.aarch64_accessors = count;

	for (accessor_index = 0; accessor_index < model->accessor_count;
	     accessor_index++) {
		const struct orlix_tcti_register_accessor *accessor =
			&model->accessors[accessor_index];
		const struct orlix_tcti_system_accessor_form_mapping *mapping;
		struct orlix_tcti_system_accessor_reconciliation_entry *entry;

		if (!is_aarch64_system_accessor(accessor))
			continue;
		entry = &result->entries[output_index++];
		entry->accessor_index = (uint32_t)accessor_index;
		entry->encoding_index = UINT32_MAX;
		entry->condition_expression = accessor->condition_expression;
		entry->accessor_name = accessor->name;
		entry->accessor_source_offset = accessor->source_offset;
		entry->accessor_source_length = accessor->source_length;
		entry->condition_source_offset = accessor->condition_offset;
		entry->condition_source_length = accessor->condition_length;

		if (accessor->first_system_encoding == UINT32_MAX ||
		    accessor->system_encoding_count != 1U ||
		    accessor->first_system_encoding >= model->system_encoding_count ||
		    accessor->system_encoding_count >
			model->system_encoding_count - accessor->first_system_encoding) {
			entry->disposition = accessor->system_encoding_count > 1U ?
				ORLIX_TCTI_SYSTEM_ACCESSOR_AMBIGUOUS :
				ORLIX_TCTI_SYSTEM_ACCESSOR_INVALID;
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
			entry->disposition = ORLIX_TCTI_SYSTEM_ACCESSOR_UNSUPPORTED;
			count_disposition(result, entry->disposition);
			continue;
		}
		if (!entry->selector_count) {
			entry->disposition = ORLIX_TCTI_SYSTEM_ACCESSOR_INVALID;
			count_disposition(result, entry->disposition);
			continue;
		}
		if (selector_identity_for_encoding(model, entry->encoding_index,
						 &entry->selector_identity) ||
		    condition_identity_for_accessor(model, accessor,
						  &entry->condition_identity)) {
			entry->disposition = ORLIX_TCTI_SYSTEM_ACCESSOR_INVALID;
			count_disposition(result, entry->disposition);
			continue;
		}
		entry->generic_leaf = mapping->leaf;
		entry->direction = mapping->direction;
		entry->disposition = ORLIX_TCTI_SYSTEM_ACCESSOR_MAPPED;
		count_disposition(result, entry->disposition);
	}
	return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK;
}

enum orlix_tcti_system_accessor_reconciliation_error
orlix_tcti_system_accessor_reconciliation_validate(
	const struct orlix_tcti_system_accessor_reconciliation_result *result)
{
	if (!result || (!result->entries && result->entry_count) ||
	    result->census.aarch64_accessors != result->entry_count ||
	    result->census.mapped + result->census.reserved +
		result->census.privileged + result->census.unsupported +
		result->census.ambiguous + result->census.contradictory +
		result->census.invalid != result->entry_count)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT;
	if (result->census.contradictory)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_CONTRADICTORY;
	if (result->census.ambiguous)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_AMBIGUOUS;
	if (result->census.invalid)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID;
	if (result->census.unsupported)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_UNSUPPORTED;
	if (result->census.privileged)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_PRIVILEGED;
	if (result->census.reserved)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_RESERVED;
	return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK;
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

enum orlix_tcti_system_accessor_reconciliation_error
orlix_tcti_system_accessor_reconciliation_emit(
	const struct orlix_tcti_register_model *model, FILE *output)
{
	struct orlix_tcti_system_accessor_reconciliation_result result = { 0 };
	enum orlix_tcti_system_accessor_reconciliation_error error;
	size_t index;

	if (!output)
		return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT;
	error = orlix_tcti_system_accessor_reconcile(model, &result);
	if (error)
		return error;
	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated from the pinned Arm AARCHMRS 2026-06 register source. Do not edit. */\n"
		  "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(\"vFATAp1-A\", \"818\", "
		  "\"2026-06_rel\", \"2.9.5\", \"2026-06-24 17:12:14\", "
		  "\"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874\")\n",
		  output) == EOF ||
	    fprintf(output, "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(%zuU, %zuU, %zuU, "
		    "%zuU, %zuU, %zuU, %zuU, %zuU)\n",
		    result.census.aarch64_accessors, result.census.mapped,
		    result.census.reserved, result.census.privileged,
		    result.census.unsupported, result.census.ambiguous,
		    result.census.contradictory, result.census.invalid) < 0)
		goto io;
	if (fprintf(output, "ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(UINT64_C(0x%016" PRIx64 "))\n",
		    reconciliation_identity(&result)) < 0)
		goto io;
	for (index = 0; index < result.entry_count; index++) {
		const struct orlix_tcti_system_accessor_reconciliation_entry *entry =
			&result.entries[index];

		if (fprintf(output, "ORLIX_TCTI_A64_SYSTEM_ACCESSOR(%" PRIu32 "U, "
		    "%" PRIu32 "U, ", entry->accessor_index,
		    entry->encoding_index) < 0 ||
		    emit_c_string(output, entry->accessor_name) ||
		    fprintf(output, ", ") < 0 ||
		    emit_c_string(output,
			 orlix_tcti_system_accessor_generic_leaf_name(entry->generic_leaf)) ||
		    fprintf(output, ", %uU, %uU, %uU, %" PRIu32 "U, "
		    "UINT64_C(0x%016" PRIx64 "), UINT64_C(0x%016" PRIx64 "), "
		    "%zuU, %zuU, %zuU, %zuU, %zuU, %zuU)\n",
		    (unsigned int)entry->direction,
		    (unsigned int)entry->disposition, entry->selector_count,
		    entry->condition_expression, entry->selector_identity,
		    entry->condition_identity, entry->accessor_source_offset,
		    entry->accessor_source_length, entry->encoding_source_offset,
		    entry->encoding_source_length, entry->condition_source_offset,
		    entry->condition_source_length) < 0)
			goto io;
	}
	orlix_tcti_system_accessor_reconciliation_destroy(&result);
	return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK;
io:
	orlix_tcti_system_accessor_reconciliation_destroy(&result);
	return ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID;
}

void orlix_tcti_system_accessor_reconciliation_destroy(
	struct orlix_tcti_system_accessor_reconciliation_result *result)
{
	if (!result)
		return;
	free(result->entries);
	memset(result, 0, sizeof(*result));
}

const char *orlix_tcti_system_accessor_generic_leaf_name(
	enum orlix_tcti_system_accessor_generic_leaf leaf)
{
	switch (leaf) {
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MRS_RS_SYSTEMMOVE:
		return "MRS_RS_systemmove";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SI_PSTATE:
		return "MSR_SI_pstate";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SR_SYSTEMMOVE:
		return "MSR_SR_systemmove";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS:
		return "SYS_CR_systeminstrs";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYSL_RC_SYSTEMINSTRS:
		return "SYSL_RC_systeminstrs";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYSP_CR_SYSPAIRINSTRS:
		return "SYSP_CR_syspairinstrs";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSRR_SR_SYSTEMMOVEPR:
		return "MSRR_SR_systemmovepr";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MRRS_RS_SYSTEMMOVEPR:
		return "MRRS_RS_systemmovepr";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_NONE:
	default:
		return "";
	}
}

const char *orlix_tcti_system_accessor_reconciliation_error_name(
	enum orlix_tcti_system_accessor_reconciliation_error error)
{
	switch (error) {
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK:
		return "ok";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT:
		return "invalid argument";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY:
		return "out of memory";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_RESERVED:
		return "reserved accessor";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_PRIVILEGED:
		return "privileged accessor";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_UNSUPPORTED:
		return "unsupported accessor form";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_AMBIGUOUS:
		return "ambiguous accessor encoding";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_CONTRADICTORY:
		return "contradictory accessor metadata";
	case ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID:
		return "invalid accessor encoding";
	default:
		return "unknown error";
	}
}
