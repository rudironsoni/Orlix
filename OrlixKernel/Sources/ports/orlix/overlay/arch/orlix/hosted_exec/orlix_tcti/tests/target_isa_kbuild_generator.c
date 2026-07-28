/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Kbuild host generator for the canonical AArch64 target ledgers.
 *
 * This program consumes C .def inputs compiled into the host tool. It has no
 * JSON parser, source path, runtime profile, or HWCAP input.
 */
#include "target_isa_kbuild_generator.h"

#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct source_string_offsets {
	uint32_t name;
	uint32_t mnemonic;
	uint32_t operation;
	uint32_t condition;
};

struct classification_string_offsets {
	uint32_t name;
	uint32_t canonical;
	uint32_t evidence;
	uint32_t proof;
};

struct system_accessor_string_offsets {
	uint32_t name;
	uint32_t generic_leaf;
};

struct generated_offsets {
	uint32_t metadata[6];
	uint32_t accessor_metadata[6];
	struct source_string_offsets *source;
	struct classification_string_offsets *classification;
	struct system_accessor_string_offsets *accessors;
	uint32_t pool_size;
};

static int fixed_text(const char *actual, const char *expected)
{
	return actual && !strcmp(actual, expected);
}

static int valid_sha256(const char *text)
{
	size_t index;

	if (!text || strlen(text) != 64U)
		return 0;
	for (index = 0; index < 64U; index++)
		if (!((text[index] >= '0' && text[index] <= '9') ||
		      (text[index] >= 'a' && text[index] <= 'f')))
			return 0;
	return 1;
}

static int valid_condition(const char *text)
{
	size_t index;
	size_t length;

	if (!text)
		return 0;
	length = strlen(text);
	if (length < 10U || (length & 1U) ||
	    strncmp(text, "54434e44", 8U))
		return 0;
	for (index = 0; index < length; index++)
		if (!((text[index] >= '0' && text[index] <= '9') ||
		      (text[index] >= 'a' && text[index] <= 'f')))
			return 0;
	return 1;
}

static uint64_t accessor_identity_byte(uint64_t identity, unsigned char byte)
{
	return (identity ^ byte) * UINT64_C(1099511628211);
}

static uint64_t accessor_identity_u64(uint64_t identity, uint64_t value)
{
	unsigned int index;

	for (index = 0; index < 8U; index++)
		identity = accessor_identity_byte(identity,
			(unsigned char)(value >> (index * 8U)));
	return identity;
}

static uint64_t accessor_identity_text(uint64_t identity, const char *text)
{
	size_t length = strlen(text);
	size_t index;

	identity = accessor_identity_u64(identity, length);
	for (index = 0; index < length; index++)
		identity = accessor_identity_byte(identity, (unsigned char)text[index]);
	return identity;
}

uint64_t orlix_tcti_a64_kbuild_system_accessor_identity(
	const struct orlix_tcti_a64_kbuild_system_accessor_row *accessors,
	size_t accessor_count)
{
	uint64_t identity = UINT64_C(1469598103934665603);
	size_t index;

	identity = accessor_identity_u64(identity, accessor_count);
	for (index = 0; index < accessor_count; index++) {
		const struct orlix_tcti_a64_kbuild_system_accessor_row *row =
			&accessors[index];

		identity = accessor_identity_u64(identity, row->accessor_index);
		identity = accessor_identity_u64(identity, row->encoding_index);
		identity = accessor_identity_text(identity, row->name);
		identity = accessor_identity_text(identity, row->variant_name);
		identity = accessor_identity_text(identity, row->generic_leaf);
		identity = accessor_identity_u64(identity, row->direction);
		identity = accessor_identity_u64(identity, row->disposition);
		identity = accessor_identity_u64(identity, row->selector_count);
		identity = accessor_identity_u64(identity, row->condition_expression);
		identity = accessor_identity_u64(identity, row->access_expression);
		identity = accessor_identity_u64(identity, row->concrete_selector);
		identity = accessor_identity_u64(identity, row->selector_identity);
		identity = accessor_identity_u64(identity, row->condition_identity);
		identity = accessor_identity_u64(identity, row->access_identity);
		identity = accessor_identity_u64(identity, row->applicability);
		identity = accessor_identity_u64(identity, row->semantics);
		identity = accessor_identity_u64(identity, row->implementation);
		identity = accessor_identity_u64(identity, row->proof_state);
		identity = accessor_identity_text(identity, row->decoder_owner);
		identity = accessor_identity_text(identity, row->execution_owner);
		identity = accessor_identity_text(identity, row->kunit_suite);
		identity = accessor_identity_text(identity, row->kunit_case);
		identity = accessor_identity_u64(identity,
			row->accessor_source_offset);
		identity = accessor_identity_u64(identity,
			row->accessor_source_length);
		identity = accessor_identity_u64(identity,
			row->encoding_source_offset);
		identity = accessor_identity_u64(identity,
			row->encoding_source_length);
		identity = accessor_identity_u64(identity,
			row->condition_source_offset);
		identity = accessor_identity_u64(identity,
			row->condition_source_length);
		identity = accessor_identity_u64(identity, row->access_source_offset);
		identity = accessor_identity_u64(identity, row->access_source_length);
	}
	return identity;
}

static size_t find_source_row(const struct orlix_tcti_a64_kbuild_source_row *source,
				      size_t source_count, const char *name)
{
	size_t index;

	if (!name || !name[0])
		return source_count;
	for (index = 0; index < source_count; index++)
		if (source[index].name && !strcmp(source[index].name, name))
			return index;
	return source_count;
}

/*
 * The source schema declares aliases through operation and instruction-alias
 * objects. Those objects can deliberately resolve operations with different
 * names and constrained encodings, so source-row identity is not an alias
 * predicate. The inventory ledger instead records the reviewed direct edge,
 * which must terminate at one classified canonical owner.
 */
static bool valid_terminal_relationship(
	const struct orlix_tcti_a64_kbuild_source_row *source,
	const struct orlix_tcti_a64_kbuild_classification_row *classification,
	size_t source_count, size_t row_index)
{
	const struct orlix_tcti_a64_kbuild_classification_row *row =
		&classification[row_index];
	const struct orlix_tcti_a64_kbuild_classification_row *canonical;
	size_t canonical_index;

	if (row->classification != ORLIX_TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE)
		return row->relation == ORLIX_TCTI_A64_KBUILD_RELATION_NONE &&
		       !row->canonical[0];
	if (row->relation != ORLIX_TCTI_A64_KBUILD_RELATION_ALIAS &&
	    row->relation != ORLIX_TCTI_A64_KBUILD_RELATION_DUPLICATE)
		return false;
	if (!row->canonical[0] || !row->evidence[0] || !row->proof[0] ||
	    !strcmp(row->name, row->canonical))
		return false;
	canonical_index = find_source_row(source, source_count, row->canonical);
	if (canonical_index == source_count)
		return false;
	canonical = &classification[canonical_index];
	if (canonical->classification == ORLIX_TCTI_A64_KBUILD_UNCLASSIFIED ||
	    canonical->classification == ORLIX_TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE ||
	    canonical->relation != ORLIX_TCTI_A64_KBUILD_RELATION_NONE ||
	    canonical->canonical[0] || !canonical->evidence[0] ||
	    !canonical->proof[0])
		return false;
	return true;
}

static enum orlix_tcti_a64_kbuild_generator_error validate_inputs(
	const struct orlix_tcti_a64_kbuild_metadata *metadata,
	const struct orlix_tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct orlix_tcti_a64_kbuild_classification_row *classification,
	size_t classification_count,
	const struct orlix_tcti_a64_kbuild_system_accessor_metadata *accessor_metadata,
	const struct orlix_tcti_a64_kbuild_system_accessor_row *accessors,
	size_t accessor_count)
{
	size_t index;
	size_t prior;

	if (!metadata || !source || !classification || !accessor_metadata ||
	    !accessors)
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT;
	if (!fixed_text(metadata->architecture,
			ORLIX_TCTI_A64_KBUILD_ARCHITECTURE) ||
	    !fixed_text(metadata->build, ORLIX_TCTI_A64_KBUILD_BUILD) ||
	    !fixed_text(metadata->reference, ORLIX_TCTI_A64_KBUILD_REFERENCE) ||
	    !fixed_text(metadata->schema, ORLIX_TCTI_A64_KBUILD_SCHEMA) ||
	    !fixed_text(metadata->instructions_sha256,
			ORLIX_TCTI_A64_KBUILD_INSTRUCTIONS_SHA256) ||
	    !fixed_text(metadata->timestamp,
			ORLIX_TCTI_A64_KBUILD_INSTRUCTIONS_TIMESTAMP) ||
	    !valid_sha256(metadata->instructions_sha256) ||
	    metadata->source_byte_length !=
			ORLIX_TCTI_A64_KBUILD_INSTRUCTIONS_SOURCE_BYTES ||
	    metadata->source_count != ORLIX_TCTI_A64_KBUILD_SOURCE_COUNT)
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH;
	if (source_count != ORLIX_TCTI_A64_KBUILD_SOURCE_COUNT ||
	    classification_count != ORLIX_TCTI_A64_KBUILD_SOURCE_COUNT)
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH;
	for (index = 0; index < source_count; index++) {
		const struct orlix_tcti_a64_kbuild_source_row *source_row =
			&source[index];
		const struct orlix_tcti_a64_kbuild_classification_row *class_row =
			&classification[index];

		if (source_row->ordinal != index)
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_ORDINAL;
		if (!source_row->name || !source_row->name[0] ||
		    !source_row->mnemonic || !source_row->mnemonic[0] ||
		    !source_row->operation || !source_row->operation[0] ||
		    !valid_condition(source_row->condition_tcnd_hex) ||
		    !source_row->source_length ||
		    source_row->source_offset >= metadata->source_byte_length ||
		    source_row->source_length >
			    metadata->source_byte_length -
				    source_row->source_offset ||
		    (source_row->pattern & ~source_row->mask))
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW;
		if (!class_row->name ||
		    strcmp(source_row->name, class_row->name))
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_CLASSIFICATION_MISMATCH;
		if (class_row->classification < ORLIX_TCTI_A64_KBUILD_UNCLASSIFIED ||
		    class_row->classification >
		    ORLIX_TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE ||
		    class_row->relation < ORLIX_TCTI_A64_KBUILD_RELATION_NONE ||
		    class_row->relation > ORLIX_TCTI_A64_KBUILD_RELATION_DUPLICATE ||
		    !class_row->canonical || !class_row->evidence ||
		    !class_row->proof)
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if (class_row->relation == ORLIX_TCTI_A64_KBUILD_RELATION_NONE &&
		    class_row->canonical[0])
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if (class_row->relation != ORLIX_TCTI_A64_KBUILD_RELATION_NONE &&
		    !class_row->canonical[0])
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if ((class_row->classification ==
		     ORLIX_TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE) !=
		    (class_row->relation != ORLIX_TCTI_A64_KBUILD_RELATION_NONE))
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if (class_row->classification == ORLIX_TCTI_A64_KBUILD_UNCLASSIFIED &&
		    (class_row->relation != ORLIX_TCTI_A64_KBUILD_RELATION_NONE ||
		     class_row->canonical[0] || class_row->evidence[0] ||
		     class_row->proof[0]))
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		for (prior = 0; prior < index; prior++)
			if (!strcmp(source[prior].name, source_row->name))
				return ORLIX_TCTI_A64_KBUILD_GENERATOR_DUPLICATE_SOURCE;
	}
	for (index = 0; index < source_count; index++)
		if (!valid_terminal_relationship(source, classification,
						 source_count, index))
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
	if (!fixed_text(accessor_metadata->architecture,
			ORLIX_TCTI_A64_KBUILD_ARCHITECTURE) ||
	    !fixed_text(accessor_metadata->build, ORLIX_TCTI_A64_KBUILD_BUILD) ||
	    !fixed_text(accessor_metadata->reference,
			ORLIX_TCTI_A64_KBUILD_REFERENCE) ||
	    !fixed_text(accessor_metadata->schema, ORLIX_TCTI_A64_KBUILD_SCHEMA) ||
	    !fixed_text(accessor_metadata->timestamp,
			ORLIX_TCTI_A64_KBUILD_REGISTERS_TIMESTAMP) ||
	    !fixed_text(accessor_metadata->registers_sha256,
			ORLIX_TCTI_A64_KBUILD_REGISTERS_SHA256) ||
	    !valid_sha256(accessor_metadata->registers_sha256))
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_METADATA_MISMATCH;
	if (accessor_metadata->accessor_count !=
			ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT ||
	    accessor_count != ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT ||
	    accessor_metadata->semantic_count != accessor_count ||
	    accessor_metadata->source_access_semantics_count +
		    accessor_metadata->generic_leaf_semantics_count != accessor_count ||
	    accessor_metadata->implemented_count +
		    accessor_metadata->architectural_rejection_count +
		    accessor_metadata->unimplemented_rejection_count != accessor_count ||
	    accessor_metadata->concrete_selector_count +
		    accessor_metadata->symbolic_selector_count != accessor_count ||
	    accessor_metadata->proof_not_observed_count != accessor_count ||
	    accessor_metadata->mapped_count +
		    accessor_metadata->reserved_count +
		    accessor_metadata->privileged_count +
		    accessor_metadata->unsupported_count +
		    accessor_metadata->ambiguous_count +
		    accessor_metadata->contradictory_count +
		    accessor_metadata->invalid_count !=
			accessor_metadata->accessor_count)
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_COUNT_MISMATCH;
	if (accessor_metadata->mapped_count != accessor_metadata->accessor_count ||
	    accessor_metadata->reserved_count ||
	    accessor_metadata->privileged_count ||
	    accessor_metadata->unsupported_count ||
	    accessor_metadata->ambiguous_count ||
	    accessor_metadata->contradictory_count ||
	    accessor_metadata->invalid_count)
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_BLOCKER;
	for (index = 0; index < accessor_count; index++) {
		const struct orlix_tcti_a64_kbuild_system_accessor_row *row =
			&accessors[index];
		int generic_leaf_found = 0;

		if (!row->name || strncmp(row->name, "A64.", 4U) ||
		    !row->variant_name || !row->variant_name[0] ||
		    !row->generic_leaf || !row->generic_leaf[0] ||
		    row->direction <
			    ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_DIRECTION_READ ||
		    row->direction >
			    ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_DIRECTION_EXECUTE ||
		    row->disposition != ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_MAPPED ||
		    row->encoding_index == UINT32_MAX || !row->selector_count ||
		    !row->selector_identity || !row->condition_identity ||
		    !row->access_identity || row->applicability != 1U ||
		    (row->semantics != 1U && row->semantics != 2U) ||
		    (row->implementation < 1U || row->implementation > 3U) ||
		    row->proof_state != 1U || !row->decoder_owner ||
		    !row->execution_owner || !row->kunit_suite || !row->kunit_case ||
		    row->condition_expression == UINT32_MAX ||
		    !row->accessor_source_length || !row->encoding_source_length ||
		    !row->condition_source_length)
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW;
		for (prior = 0; prior < source_count; prior++)
			if (!strcmp(source[prior].name, row->generic_leaf)) {
				generic_leaf_found = 1;
				break;
			}
		if (!generic_leaf_found)
			return ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW;
		for (prior = 0; prior < index; prior++)
			if (accessors[prior].accessor_index == row->accessor_index ||
			    accessors[prior].encoding_index == row->encoding_index)
				return ORLIX_TCTI_A64_KBUILD_GENERATOR_DUPLICATE_ACCESSOR;
	}
	if (orlix_tcti_a64_kbuild_system_accessor_identity(accessors, accessor_count) !=
		    accessor_metadata->reconciliation_identity)
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_METADATA_MISMATCH;
	return ORLIX_TCTI_A64_KBUILD_GENERATOR_OK;
}

static int add_string_offset(uint64_t *pool_size, const char *text,
			     uint32_t *offset)
{
	size_t length = strlen(text) + 1U;

	if (*pool_size > UINT32_MAX ||
	    length > UINT32_MAX - *pool_size)
		return -1;
	*offset = (uint32_t)*pool_size;
	*pool_size += length;
	return 0;
}

static int build_offsets(
	const struct orlix_tcti_a64_kbuild_metadata *metadata,
	const struct orlix_tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct orlix_tcti_a64_kbuild_classification_row *classification,
	const struct orlix_tcti_a64_kbuild_system_accessor_metadata *accessor_metadata,
	const struct orlix_tcti_a64_kbuild_system_accessor_row *accessors,
	size_t accessor_count,
	struct generated_offsets *offsets)
{
	const char *const metadata_text[] = {
		metadata->architecture, metadata->build, metadata->reference,
		metadata->schema, metadata->instructions_sha256,
		metadata->timestamp,
	};
	const char *const accessor_metadata_text[] = {
		accessor_metadata->architecture, accessor_metadata->build,
		accessor_metadata->reference, accessor_metadata->schema,
		accessor_metadata->timestamp, accessor_metadata->registers_sha256,
	};
	uint64_t pool_size = 0;
	size_t index;

	offsets->source = calloc(source_count, sizeof(*offsets->source));
	offsets->classification =
		calloc(source_count, sizeof(*offsets->classification));
	offsets->accessors = calloc(accessor_count, sizeof(*offsets->accessors));
	if (!offsets->source || !offsets->classification || !offsets->accessors)
		return -1;
	for (index = 0; index < sizeof(metadata_text) / sizeof(metadata_text[0]);
	     index++)
		if (add_string_offset(&pool_size, metadata_text[index],
				      &offsets->metadata[index]))
			return -1;
	for (index = 0; index < source_count; index++)
		if (add_string_offset(&pool_size, source[index].name,
				      &offsets->source[index].name) ||
		    add_string_offset(&pool_size, source[index].mnemonic,
				      &offsets->source[index].mnemonic) ||
		    add_string_offset(&pool_size, source[index].operation,
				      &offsets->source[index].operation) ||
		    add_string_offset(&pool_size, source[index].condition_tcnd_hex,
				      &offsets->source[index].condition))
			return -1;
	for (index = 0; index < source_count; index++)
		if (add_string_offset(&pool_size, classification[index].name,
				      &offsets->classification[index].name) ||
		    add_string_offset(&pool_size, classification[index].canonical,
				      &offsets->classification[index].canonical) ||
		    add_string_offset(&pool_size, classification[index].evidence,
				      &offsets->classification[index].evidence) ||
		    add_string_offset(&pool_size, classification[index].proof,
				      &offsets->classification[index].proof))
			return -1;
	for (index = 0;
	     index < sizeof(accessor_metadata_text) /
		     sizeof(accessor_metadata_text[0]);
	     index++)
		if (add_string_offset(&pool_size, accessor_metadata_text[index],
				      &offsets->accessor_metadata[index]))
			return -1;
	for (index = 0; index < accessor_count; index++)
		if (add_string_offset(&pool_size, accessors[index].name,
				      &offsets->accessors[index].name) ||
		    add_string_offset(&pool_size, accessors[index].generic_leaf,
				      &offsets->accessors[index].generic_leaf))
			return -1;
	offsets->pool_size = (uint32_t)pool_size;
	return 0;
}

static int emit_pool_byte(FILE *output, unsigned int byte, size_t *column)
{
	if (!*column && fputs("\t", output) == EOF)
		return -1;
	if (fprintf(output, "0x%02xU,", byte) < 0)
		return -1;
	(*column)++;
	if (*column == 12U) {
		if (fputc('\n', output) == EOF)
			return -1;
		*column = 0;
	} else if (fputc(' ', output) == EOF) {
		return -1;
	}
	return 0;
}

static int emit_pool_text(FILE *output, const char *text, size_t *column)
{
	const unsigned char *cursor = (const unsigned char *)text;

	do {
		if (emit_pool_byte(output, *cursor, column))
			return -1;
	} while (*cursor++);
	return 0;
}

static int emit_header(
	const struct orlix_tcti_a64_kbuild_metadata *metadata,
	const struct orlix_tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct orlix_tcti_a64_kbuild_classification_row *classification,
	const struct orlix_tcti_a64_kbuild_system_accessor_metadata *accessor_metadata,
	const struct orlix_tcti_a64_kbuild_system_accessor_row *accessors,
	size_t accessor_count,
	const struct generated_offsets *offsets,
	FILE *output)
{
	const char *const metadata_text[] = {
		metadata->architecture, metadata->build, metadata->reference,
		metadata->schema, metadata->instructions_sha256,
		metadata->timestamp,
	};
	const char *const accessor_metadata_text[] = {
		accessor_metadata->architecture, accessor_metadata->build,
		accessor_metadata->reference, accessor_metadata->schema,
		accessor_metadata->timestamp, accessor_metadata->registers_sha256,
	};
	size_t unclassified_count = 0;
	size_t index;
	size_t column = 0;

	for (index = 0; index < source_count; index++)
		if (classification[index].classification ==
		    ORLIX_TCTI_A64_KBUILD_UNCLASSIFIED)
			unclassified_count++;
	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated by target_isa_kbuild_generator.c. Do not edit. */\n"
		  "#ifndef ORLIX_TCTI_A64_TARGET_GENERATED_H\n"
		  "#define ORLIX_TCTI_A64_TARGET_GENERATED_H\n"
		  "#include <linux/types.h>\n"
		  "#define ORLIX_TCTI_A64_GENERATED_ARCHITECTURE \"vFATAp1-A\"\n"
		  "#define ORLIX_TCTI_A64_GENERATED_BUILD \"818\"\n"
		  "#define ORLIX_TCTI_A64_GENERATED_REFERENCE \"2026-06_rel\"\n"
		  "#define ORLIX_TCTI_A64_GENERATED_SCHEMA \"2.9.5\"\n"
		  "#define ORLIX_TCTI_A64_GENERATED_INSTRUCTIONS_SHA256 "
		  "\"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\"\n"
		  "#define ORLIX_TCTI_A64_GENERATED_REGISTERS_SHA256 "
		  "\"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874\"\n"
		  "#define ORLIX_TCTI_A64_GENERATED_SOURCE_COUNT 4350U\n"
		  "#define ORLIX_TCTI_A64_GENERATED_SYSTEM_ACCESSOR_COUNT 2014U\n",
		  output) == EOF ||
	    fprintf(output,
		    "#define ORLIX_TCTI_A64_GENERATED_UNCLASSIFIED_COUNT %zuU\n"
		    "#define ORLIX_TCTI_A64_GENERATED_CLASSIFICATION_COMPLETE %uU\n"
		    "/* ISA completion requires separate source-bound semantic proof. */\n"
		    "#define ORLIX_TCTI_A64_GENERATED_ISA_COMPLETE 0U\n"
		    "#define ORLIX_TCTI_A64_GENERATED_STRING_POOL_SIZE %" PRIu32 "U\n",
		    unclassified_count, unclassified_count ? 0U : 1U,
		    offsets->pool_size) < 0 ||
	    fputs("struct orlix_tcti_a64_generated_metadata {\n"
		  "\tu32 architecture; u32 build; u32 reference; u32 schema;\n"
		  "\tu32 instructions_sha256; u32 timestamp;\n"
		  "\tu32 source_byte_length; u32 source_count;\n"
		  "};\n"
		  "struct orlix_tcti_a64_generated_row {\n"
		  "\tu32 ordinal; u32 name; u32 mnemonic; u32 operation;\n"
		  "\tu32 mask; u32 pattern; u32 condition;\n"
		  "\tu32 source_offset; u32 source_length;\n"
		  "\tu32 classification_name; u32 canonical; u32 evidence; u32 proof;\n"
		  "\tu8 classification; u8 relation; u8 reserved[2];\n"
		  "};\n"
		  "struct orlix_tcti_a64_generated_system_accessor_metadata {\n"
		  "\tu32 architecture; u32 build; u32 reference; u32 schema;\n"
		  "\tu32 timestamp; u32 registers_sha256; u32 accessor_count;\n"
		  "\tu32 mapped_count; u32 reserved_count; u32 privileged_count;\n"
		  "\tu32 unsupported_count; u32 ambiguous_count;\n"
		  "\tu32 contradictory_count; u32 invalid_count;\n"
		  "\tu64 reconciliation_identity;\n"
		  "};\n"
		  "struct orlix_tcti_a64_generated_system_accessor_row {\n"
		  "\tu32 accessor_index; u32 encoding_index; u32 name;\n"
		  "\tu32 generic_leaf; u32 selector_count; u32 condition_expression;\n"
		  "\tu64 selector_identity; u64 condition_identity;\n"
		  "\tu32 accessor_source_offset; u32 accessor_source_length;\n"
		  "\tu32 encoding_source_offset; u32 encoding_source_length;\n"
		  "\tu32 condition_source_offset; u32 condition_source_length;\n"
		  "\tu8 direction; u8 disposition; u8 reserved[2];\n"
		  "};\n"
		  "static const u8 orlix_tcti_a64_generated_strings[] = {\n",
		  output) == EOF)
		return -1;
	for (index = 0; index < sizeof(metadata_text) / sizeof(metadata_text[0]);
	     index++)
		if (emit_pool_text(output, metadata_text[index], &column))
			return -1;
	for (index = 0; index < source_count; index++)
		if (emit_pool_text(output, source[index].name, &column) ||
		    emit_pool_text(output, source[index].mnemonic, &column) ||
		    emit_pool_text(output, source[index].operation, &column) ||
		    emit_pool_text(output, source[index].condition_tcnd_hex, &column))
			return -1;
	for (index = 0; index < source_count; index++)
		if (emit_pool_text(output, classification[index].name, &column) ||
		    emit_pool_text(output, classification[index].canonical, &column) ||
		    emit_pool_text(output, classification[index].evidence, &column) ||
		    emit_pool_text(output, classification[index].proof, &column))
			return -1;
	for (index = 0;
	     index < sizeof(accessor_metadata_text) /
		     sizeof(accessor_metadata_text[0]);
	     index++)
		if (emit_pool_text(output, accessor_metadata_text[index], &column))
			return -1;
	for (index = 0; index < accessor_count; index++)
		if (emit_pool_text(output, accessors[index].name, &column) ||
		    emit_pool_text(output, accessors[index].generic_leaf, &column))
			return -1;
	if (column && fputc('\n', output) == EOF)
		return -1;
	if (fprintf(output,
		    "};\n"
		    "static const struct orlix_tcti_a64_generated_metadata "
		    "orlix_tcti_a64_generated_metadata = {\n"
		    "\t%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %"
		    PRIu32 "U, %" PRIu32 "U, %" PRIu32
		    "U, 115441429U, 4350U,\n};\n"
		    "static const struct orlix_tcti_a64_generated_row "
		    "orlix_tcti_a64_generated_rows[4350] = {\n",
		    offsets->metadata[0], offsets->metadata[1],
		    offsets->metadata[2], offsets->metadata[3],
		    offsets->metadata[4], offsets->metadata[5]) < 0)
		return -1;
	for (index = 0; index < source_count; index++) {
		const struct source_string_offsets *source_strings =
			&offsets->source[index];
		const struct classification_string_offsets *class_strings =
			&offsets->classification[index];

		if (fprintf(output,
			    "\t{ %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, 0x%08" PRIx32
			    "U, 0x%08" PRIx32 "U, %" PRIu32 "U, %"
			    PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %"
			    PRIu32 "U, %" PRIu32 "U, %"
			    PRIu32 "U, %uU, %uU, { 0U, 0U } },\n",
			    source[index].ordinal, source_strings->name,
			    source_strings->mnemonic, source_strings->operation,
			    source[index].mask, source[index].pattern,
			    source_strings->condition, source[index].source_offset,
			    source[index].source_length, class_strings->name,
			    class_strings->canonical, class_strings->evidence,
			    class_strings->proof,
			    (unsigned int)classification[index].classification,
			    (unsigned int)classification[index].relation) < 0)
			return -1;
	}
	if (fprintf(output,
		    "};\n"
		    "static const struct orlix_tcti_a64_generated_system_accessor_metadata "
		    "orlix_tcti_a64_generated_system_accessor_metadata = {\n"
		    "\t%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %"
		    PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %uU, %uU, %uU, "
		    "%uU, %uU, %uU, %uU, %uU, 0x%016" PRIx64 "ULL,\n};\n"
		    "static const struct orlix_tcti_a64_generated_system_accessor_row "
		    "orlix_tcti_a64_generated_system_accessors[2014] = {\n",
		    offsets->accessor_metadata[0], offsets->accessor_metadata[1],
		    offsets->accessor_metadata[2], offsets->accessor_metadata[3],
		    offsets->accessor_metadata[4], offsets->accessor_metadata[5],
		    accessor_metadata->accessor_count,
		    accessor_metadata->mapped_count,
		    accessor_metadata->reserved_count,
		    accessor_metadata->privileged_count,
		    accessor_metadata->unsupported_count,
		    accessor_metadata->ambiguous_count,
		    accessor_metadata->contradictory_count,
		    accessor_metadata->invalid_count,
		    accessor_metadata->reconciliation_identity) < 0)
		return -1;
	for (index = 0; index < accessor_count; index++) {
		const struct system_accessor_string_offsets *strings =
			&offsets->accessors[index];
		const struct orlix_tcti_a64_kbuild_system_accessor_row *row =
			&accessors[index];

		if (fprintf(output,
			    "\t{ %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
		    "U, 0x%016" PRIx64 "ULL, 0x%016" PRIx64
		    "ULL, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
		    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
		    "U, %uU, %uU, { 0U, 0U } },\n",
			    row->accessor_index, row->encoding_index,
			    strings->name, strings->generic_leaf,
		    row->selector_count, row->condition_expression,
		    row->selector_identity, row->condition_identity,
			    row->accessor_source_offset,
			    row->accessor_source_length,
			    row->encoding_source_offset,
		    row->encoding_source_length,
		    row->condition_source_offset,
		    row->condition_source_length,
			    (unsigned int)row->direction,
			    (unsigned int)row->disposition) < 0)
			return -1;
	}
	return fputs("};\n"
		     "#endif /* ORLIX_TCTI_A64_TARGET_GENERATED_H */\n",
		     output) == EOF || ferror(output) ? -1 : 0;
}

enum orlix_tcti_a64_kbuild_generator_error orlix_tcti_a64_kbuild_generate_header(
	const struct orlix_tcti_a64_kbuild_metadata *metadata,
	const struct orlix_tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct orlix_tcti_a64_kbuild_classification_row *classification,
	size_t classification_count,
	const struct orlix_tcti_a64_kbuild_system_accessor_metadata *accessor_metadata,
	const struct orlix_tcti_a64_kbuild_system_accessor_row *accessors,
	size_t accessor_count,
	FILE *output)
{
	struct generated_offsets offsets = { 0 };
	enum orlix_tcti_a64_kbuild_generator_error error;

	if (!output)
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT;
	error = validate_inputs(metadata, source, source_count, classification,
				classification_count, accessor_metadata, accessors,
				accessor_count);
	if (error != ORLIX_TCTI_A64_KBUILD_GENERATOR_OK)
		return error;
	if (build_offsets(metadata, source, source_count, classification,
			  accessor_metadata, accessors, accessor_count, &offsets)) {
		free(offsets.source);
		free(offsets.classification);
		free(offsets.accessors);
		return ORLIX_TCTI_A64_KBUILD_GENERATOR_NO_MEMORY;
	}
	error = emit_header(metadata, source, source_count, classification,
			    accessor_metadata, accessors, accessor_count, &offsets,
			    output) ?
		ORLIX_TCTI_A64_KBUILD_GENERATOR_IO : ORLIX_TCTI_A64_KBUILD_GENERATOR_OK;
	free(offsets.source);
	free(offsets.classification);
	free(offsets.accessors);
	return error;
}

const char *orlix_tcti_a64_kbuild_generator_error_name(
	enum orlix_tcti_a64_kbuild_generator_error error)
{
	switch (error) {
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_OK:
		return "success";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT:
		return "invalid argument";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH:
		return "metadata mismatch";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH:
		return "count mismatch";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_ORDINAL:
		return "bad ordinal";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW:
		return "bad source row";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_DUPLICATE_SOURCE:
		return "duplicate source row";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_CLASSIFICATION_MISMATCH:
		return "classification identity mismatch";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION:
		return "bad classification";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_METADATA_MISMATCH:
		return "system accessor metadata mismatch";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_COUNT_MISMATCH:
		return "system accessor count mismatch";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW:
		return "bad system accessor row";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_DUPLICATE_ACCESSOR:
		return "duplicate system accessor";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_BLOCKER:
		return "unmapped system accessor blocker";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_NO_MEMORY:
		return "out of memory";
	case ORLIX_TCTI_A64_KBUILD_GENERATOR_IO:
		return "I/O failure";
	}
	return "unknown failure";
}

#ifndef ORLIX_TCTI_A64_KBUILD_GENERATOR_NO_BUILTIN_INPUT
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(architecture, build, reference, schema, \
					sha256, count, timestamp_value, \
					source_byte_length_value) \
	static const struct orlix_tcti_a64_kbuild_metadata builtin_metadata = { \
		architecture, build, reference, schema, sha256, timestamp_value, \
		source_byte_length_value, count \
	};
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(...)
#include "../isa/source_manifest.def"
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, \
				     pattern, condition, source_offset_value, \
				     source_length_value) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition, \
	  source_offset_value, source_length_value },
static const struct orlix_tcti_a64_kbuild_source_row builtin_source[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

#define ORLIX_TCTI_A64_KBUILD_CLASS_UNCLASSIFIED ORLIX_TCTI_A64_KBUILD_UNCLASSIFIED
#define ORLIX_TCTI_A64_KBUILD_CLASS_REQUIRED ORLIX_TCTI_A64_KBUILD_REQUIRED
#define ORLIX_TCTI_A64_KBUILD_CLASS_NON_EL0 ORLIX_TCTI_A64_KBUILD_NON_EL0
#define ORLIX_TCTI_A64_KBUILD_CLASS_ARCHITECTURALLY_UNDEFINED \
	ORLIX_TCTI_A64_KBUILD_ARCHITECTURALLY_UNDEFINED
#define ORLIX_TCTI_A64_KBUILD_CLASS_ALIAS_OR_DUPLICATE \
	ORLIX_TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE
#define ORLIX_TCTI_A64_KBUILD_REL_ORLIX_TCTI_A64_TARGET_RELATION_NONE \
	ORLIX_TCTI_A64_KBUILD_RELATION_NONE
#define ORLIX_TCTI_A64_KBUILD_REL_ORLIX_TCTI_A64_TARGET_RELATION_ALIAS \
	ORLIX_TCTI_A64_KBUILD_RELATION_ALIAS
#define ORLIX_TCTI_A64_KBUILD_REL_ORLIX_TCTI_A64_TARGET_RELATION_DUPLICATE \
	ORLIX_TCTI_A64_KBUILD_RELATION_DUPLICATE
#define ORLIX_TCTI_A64_KBUILD_STRING_(value) #value
#define ORLIX_TCTI_A64_KBUILD_STRING(value) ORLIX_TCTI_A64_KBUILD_STRING_(value)
#define ORLIX_TCTI_A64_KBUILD_CLASS_(value) ORLIX_TCTI_A64_KBUILD_CLASS_##value
#define ORLIX_TCTI_A64_KBUILD_CLASS(value) ORLIX_TCTI_A64_KBUILD_CLASS_(value)
#define ORLIX_TCTI_A64_KBUILD_REL_(value) ORLIX_TCTI_A64_KBUILD_REL_##value
#define ORLIX_TCTI_A64_KBUILD_REL(value) ORLIX_TCTI_A64_KBUILD_REL_(value)
#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
				       canonical, evidence, proof) \
	{ ORLIX_TCTI_A64_KBUILD_STRING(name), ORLIX_TCTI_A64_KBUILD_CLASS(classification), \
	  ORLIX_TCTI_A64_KBUILD_REL(relation), canonical, evidence, proof },
static const struct orlix_tcti_a64_kbuild_classification_row
builtin_classification[] = {
#include "../isa/target_classification.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION

#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(architecture_value, build_value, \
					reference_value, schema_value, \
					timestamp_value, sha256_value) \
	static const char builtin_accessor_architecture[] = architecture_value; \
	static const char builtin_accessor_build[] = build_value; \
	static const char builtin_accessor_reference[] = reference_value; \
	static const char builtin_accessor_schema[] = schema_value; \
	static const char builtin_accessor_timestamp[] = timestamp_value; \
	static const char builtin_accessor_sha256[] = sha256_value;
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(total, mapped, reserved, privileged, \
					unsupported, ambiguous, contradictory, \
					invalid) \
	static const uint32_t builtin_accessor_counts[] = { \
		total, mapped, reserved, privileged, unsupported, ambiguous, \
		contradictory, invalid \
	};
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS(total, source, generic, \
		implemented, architectural, unimplemented, concrete, symbolic, proof) \
	static const uint32_t builtin_accessor_semantic_counts[] = { \
		total, source, generic, implemented, architectural, unimplemented, \
		concrete, symbolic, proof \
	};
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(identity_value) \
	static const uint64_t builtin_accessor_identity = identity_value;
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(...)
#include "../isa/target_system_accessor_reconciliation.def"
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE

static const struct orlix_tcti_a64_kbuild_system_accessor_metadata
builtin_accessor_metadata = {
	builtin_accessor_architecture, builtin_accessor_build,
	builtin_accessor_reference, builtin_accessor_schema,
	builtin_accessor_timestamp, builtin_accessor_sha256,
	builtin_accessor_counts[0], builtin_accessor_counts[1],
	builtin_accessor_counts[2], builtin_accessor_counts[3],
	builtin_accessor_counts[4], builtin_accessor_counts[5],
	builtin_accessor_counts[6], builtin_accessor_counts[7],
	builtin_accessor_semantic_counts[0], builtin_accessor_semantic_counts[1],
	builtin_accessor_semantic_counts[2], builtin_accessor_semantic_counts[3],
	builtin_accessor_semantic_counts[4], builtin_accessor_semantic_counts[5],
	builtin_accessor_semantic_counts[6], builtin_accessor_semantic_counts[7],
	builtin_accessor_semantic_counts[8],
	builtin_accessor_identity,
};

#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(accessor, encoding, name_value, variant, \
		generic, direction_value, disposition_value, selectors, condition, access, \
		concrete, applicability, semantics, implementation, proof, \
		selector_identity_value, condition_identity_value, access_identity, \
		decoder, executor, suite, test_case, accessor_offset, accessor_length, \
		encoding_offset, encoding_length, condition_offset, condition_length, \
		access_offset, access_length) \
	{ accessor, encoding, name_value, variant, generic, direction_value, \
	  disposition_value, selectors, condition, access, concrete, applicability, \
	  semantics, implementation, proof, selector_identity_value, \
	  condition_identity_value, access_identity, decoder, executor, suite, \
	  test_case, accessor_offset, accessor_length, encoding_offset, encoding_length, \
	  condition_offset, condition_length, access_offset, access_length },
static const struct orlix_tcti_a64_kbuild_system_accessor_row builtin_accessors[] = {
#include "../isa/target_system_accessor_reconciliation.def"
};
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE

#ifndef ORLIX_TCTI_A64_KBUILD_GENERATOR_NO_MAIN
int main(int argc, char **argv)
{
	enum orlix_tcti_a64_kbuild_generator_error error;

	(void)argv;
	if (argc != 1) {
		fprintf(stderr, "usage: target_isa_kbuild_generator > target.h\n");
		return EXIT_FAILURE;
	}
	error = orlix_tcti_a64_kbuild_generate_header(
		&builtin_metadata, builtin_source,
		sizeof(builtin_source) / sizeof(builtin_source[0]),
		builtin_classification,
		sizeof(builtin_classification) / sizeof(builtin_classification[0]),
		&builtin_accessor_metadata, builtin_accessors,
		sizeof(builtin_accessors) / sizeof(builtin_accessors[0]),
		stdout);
	if (error != ORLIX_TCTI_A64_KBUILD_GENERATOR_OK) {
		fprintf(stderr, "target ISA Kbuild generator: %s\n",
			orlix_tcti_a64_kbuild_generator_error_name(error));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
#endif
#endif
